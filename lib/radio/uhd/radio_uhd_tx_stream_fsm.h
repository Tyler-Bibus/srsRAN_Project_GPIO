/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "srsran/adt/optional.h"

#include <condition_variable>
#include <mutex>

#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wall"
#else // __clang__
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif // __clang__
#include <uhd/types/metadata.hpp>
#include <uhd/types/time_spec.hpp>
#pragma GCC diagnostic pop

namespace srsran {

class radio_uhd_tx_stream_fsm
{
private:
  /// Wait for end-of-burst acknowledgement timeout in seconds.
  static constexpr double WAIT_EOB_ACK_TIMEOUT_S = 0.01;

  /// Defines the Tx stream internal states.
  enum class states {
    /// Indicates the stream was not initialized successfully.
    UNINITIALIZED = 0,
    /// Indicates the stream is ready to start burst.
    START_BURST,
    /// Indicates the stream is transmitting a burst.
    IN_BURST,
    /// Indicates an end-of-burst must be transmitted and abort any transmission.
    UNDERFLOW_RECOVERY,
    /// Indicates wait for end-of-burst acknowledgement. Used when recovering
    /// from an underflow.
    WAIT_END_OF_BURST,
    // TODO: Figure out how this sob/eob meshes w/ handling underflow
    /// State that we're in while not transmitting a burst.
    IDLE,
    /// Signals a stop to the asynchronous thread.
    WAIT_STOP,
    /// Indicates the asynchronous thread is notify_stop.
    STOPPED
  };

  /// Indicates the current state.
  states state;

  // TODO: It would be better to do this with vectors
  /// Samples remaining until start of the burst
  optional<unsigned> start_of_burst;
  /// Samples remaining until of this burst (used to calculated eob)
  optional<unsigned> end_of_burst;

  /// Protects the class concurrent access.
  mutable std::mutex mutex;
  /// Condition variable to wait for certain states.
  std::condition_variable cvar;

  uhd::time_spec_t wait_eob_timeout = uhd::time_spec_t();

public:
  /// \brief Notifies that the transmit stream has been initialized successfully.
  void init_successful()
  {
    std::unique_lock<std::mutex> lock(mutex);
    state = states::IDLE;
  }

  /// \brief Notifies a late or an underflow event.
  /// \remark Transitions state end of burst if it is in a burst.
  /// \param[in] time_spec Indicates the time the underflow event occurred.
  void async_event_late_underflow(const uhd::time_spec_t& time_spec)
  {
    std::unique_lock<std::mutex> lock(mutex);
    // TODO: Handle FDD
    start_of_burst.reset();
    end_of_burst.reset();
    if (state == states::IN_BURST) {
      state            = states::UNDERFLOW_RECOVERY;
      wait_eob_timeout = time_spec;
      wait_eob_timeout += WAIT_EOB_ACK_TIMEOUT_S;
    }
  }

  /// \brief Notifies an end-of-burst acknowledgement.
  /// \remark Transitions state to start burst if it is waiting for the end-of-burst.
  void async_event_end_of_burst_ack()
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state == states::WAIT_END_OF_BURST) {
      // TODO: Handle FDD
      state = states::IDLE;
    }
  }

  /// \brief Notifies a new block transmission.
  /// \param[out] metadata Provides the destination of the required metadata.
  /// \param[in] time_spec Indicates the transmission time.
  /// \param[inout] num_samples In - number of samples available to transmit.
  ///           Out - number of samples to deal with (transmit or discard).
  /// \return True if the block shall be transmitted. False if the block shall be ignored.
  bool transmit_block(
      uhd::tx_metadata_t& metadata,
      uhd::time_spec_t& time_spec,
      unsigned& num_samples)
  {
    bool result;
    std::unique_lock<std::mutex> lock(mutex);
    // Determine how many samples are going to get pulled out
    // Also determine the metadata characteristics
    switch (state) {
      case states::UNINITIALIZED:
        num_samples = 0;
        result = false;
        break;
      case states::START_BURST:
        // Set start of burst flag and time spec.
        metadata.has_time_spec  = true;
        metadata.start_of_burst = true;
        metadata.time_spec      = time_spec;

        // Transition to in-burst.
        state = states::IN_BURST;

        if(end_of_burst) {
          if(num_samples > *end_of_burst) {
            // Only transmit up to the end of the burst
            num_samples = *end_of_burst;
            metadata.end_of_burst = true;
            state                 = states::IDLE;
            end_of_burst.reset();
          }
        }

        result = true;

        break;
      case states::IN_BURST:
        if(end_of_burst) {
          if(num_samples > *end_of_burst) {
            // Only transmit up to the end of the burst
            num_samples = *end_of_burst;
            metadata.end_of_burst = true;
            state                 = states::IDLE;
            end_of_burst.reset();
          }
        }

        result = true;

        break;
      case states::UNDERFLOW_RECOVERY:
        // Flag end-of-burst.
        metadata.end_of_burst = true;
        state                 = states::WAIT_END_OF_BURST;
        if (wait_eob_timeout == uhd::time_spec_t()) {
          wait_eob_timeout = metadata.time_spec;
          wait_eob_timeout += WAIT_EOB_ACK_TIMEOUT_S;
        }
        result = true;
      case states::WAIT_END_OF_BURST:
        num_samples = 0;
        // Consider starting the burst if the wait for end-of-burst expired.
        if (wait_eob_timeout.get_real_secs() < time_spec.get_real_secs()) {
          // Transition to idle.
          state = states::IDLE;
        }

        result = false;

        break;
      case states::IDLE:
        if(start_of_burst) {
          if(num_samples >= *start_of_burst) {
            // Consume the remaining samples before sending a start of burst
            num_samples = *start_of_burst;
            state = states::START_BURST;
            start_of_burst.reset();
          }
        }

        result = false;

        break;
      case states::WAIT_STOP:
        num_samples = 0;
        result = false;
        break;
      case states::STOPPED:
        num_samples = 0;
        result = false;
        break;
    }

    if(start_of_burst) {
      *start_of_burst -= num_samples;
    }
    if(end_of_burst) {
      *end_of_burst -= num_samples;
    }

    // Transmission shall not be ignored.
    return result;
  }

  // Name chosen for a future fix where sob/eob are vectors
  void queue_start_of_burst(unsigned start_of_burst_)
  {
    if(start_of_burst_ > 700) {
      start_of_burst_ -= 700;
    } else {
      start_of_burst_ = 0;
    }
    srsran_assert(!start_of_burst, "SoB vector not yet implemented");
    start_of_burst = start_of_burst_;
  }

  void queue_end_of_burst(unsigned end_of_burst_)
  {
    srsran_assert(!end_of_burst, "EoB vector not yet implemented");
    end_of_burst = end_of_burst_;
  }

  // TODO: I seem to have killed stop
  void stop(uhd::tx_metadata_t& metadata)
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state == states::IN_BURST) {
      metadata.end_of_burst = true;
    }
    state = states::WAIT_STOP;
  }

  bool is_stopping() const
  {
    std::unique_lock<std::mutex> lock(mutex);
    return state == states::WAIT_STOP;
  }

  void wait_stop()
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (state != states::STOPPED) {
      cvar.wait(lock);
    }
  }

  /// Notifies the asynchronous task has notify_stop.
  void async_task_stopped()
  {
    std::unique_lock<std::mutex> lock(mutex);
    state = states::STOPPED;
    cvar.notify_all();
  }
};

} // namespace srsran
