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

#include "srsran/adt/complex.h"
#include "srsran/adt/optional.h"
#include "srsran/adt/span.h"
#include "srsran/gateways/baseband/baseband_gateway_timestamp.h"

namespace srsran {

class baseband_gateway_buffer;

/// \brief Lower physical layer downlink processor - Baseband interface.
///
/// Processes baseband samples, it derives the symbol and slot timing from the number of processed samples.
class downlink_processor_baseband
{
public:
  /// Default destructor.
  virtual ~downlink_processor_baseband() = default;

  /// \brief Processes any number of baseband samples.
  ///
  /// \param[in] buffer    Baseband samples to process.
  /// \param[in] timestamp Time instant in which the first sample in the buffer is transmitted.
  /// \param[out] start    -1 if there is no need to start the TX, otherwise
  ///                       the timestamp to start the radio at.
  /// \param[out] stop     -1 if there is no need to stop the TX, otherwise
  ///                       the timestamp to stop the radio at.
  /// \remark The number of channels in \c buffer must be equal to the number of transmit ports for the sector.
  virtual void process(baseband_gateway_buffer_writer&       buffer,
                       baseband_gateway_timestamp            timestamp,
                       optional<baseband_gateway_timestamp>& start,
                       optional<baseband_gateway_timestamp>& stop) = 0;
};

} // namespace srsran
