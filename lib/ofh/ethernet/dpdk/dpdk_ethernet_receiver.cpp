/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "dpdk_ethernet_receiver.h"
#include "srsran/instrumentation/traces/ofh_traces.h"
#include "srsran/ofh/ethernet/dpdk/dpdk_ethernet_rx_buffer.h"
#include "srsran/ofh/ethernet/ethernet_frame_notifier.h"
#include "srsran/support/executors/task_executor.h"
#include <future>
#include <rte_ethdev.h>
#include <thread>

using namespace srsran;
using namespace ether;

namespace {

class dummy_frame_notifier : public frame_notifier
{
  // See interface for documentation.
  void on_new_frame(ether::unique_rx_buffer buffer) override {}
};

} // namespace

/// This dummy object is passed to the constructor of the DPDK Ethernet receiver implementation as a placeholder for the
/// actual frame notifier, which will be later set up through the \ref start() method.
static dummy_frame_notifier dummy_notifier;

dpdk_receiver_impl::dpdk_receiver_impl(task_executor&                     executor_,
                                       std::shared_ptr<dpdk_port_context> port_ctx_,
                                       srslog::basic_logger&              logger_) :
  logger(logger_), executor(executor_), notifier(dummy_notifier), port_ctx(std::move(port_ctx_))
{
  srsran_assert(port_ctx, "Invalid port context");
}

void dpdk_receiver_impl::start(frame_notifier& notifier_)
{
  notifier = std::ref(notifier_);

  std::promise<void> p;
  std::future<void>  fut = p.get_future();

  if (!executor.defer([this, &p]() {
        rx_status.store(receiver_status::running, std::memory_order_relaxed);
        // Signal start() caller thread that the operation is complete.
        p.set_value();
        receive_loop();
      })) {
    report_error("Unable to start the DPDK ethernet frame receiver on port '{}'", port_ctx->get_port_id());
  }

  // Block waiting for timing executor to start.
  fut.wait();

  logger.info("Started the DPDK ethernet frame receiver on port '{}'", port_ctx->get_port_id());
}

void dpdk_receiver_impl::stop()
{
  logger.info("Requesting stop of the DPDK ethernet frame receiver on port '{}'", port_ctx->get_port_id());
  rx_status.store(receiver_status::stop_requested, std::memory_order_relaxed);

  // Wait for the receiver thread to stop.
  while (rx_status.load(std::memory_order_acquire) != receiver_status::stopped) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  logger.info("Stopped the DPDK ethernet frame receiver on port '{}'", port_ctx->get_port_id());
}

void dpdk_receiver_impl::receive_loop()
{
  if (rx_status.load(std::memory_order_relaxed) == receiver_status::stop_requested) {
    rx_status.store(receiver_status::stopped, std::memory_order_release);
    return;
  }

  receive();

  // Retry the task deferring when it fails.
  while (!executor.defer([this]() { receive_loop(); })) {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
}

void dpdk_receiver_impl::receive()
{
  std::array<::rte_mbuf*, MAX_BURST_SIZE> mbufs;

  trace_point dpdk_rx_tp = ofh_tracer.now();
  unsigned    num_frames = ::rte_eth_rx_burst(port_ctx->get_dpdk_port_id(), 0, mbufs.data(), MAX_BURST_SIZE);
  if (num_frames == 0) {
    ofh_tracer << instant_trace_event("ofh_receiver_wait_data", instant_trace_event::cpu_scope::thread);
    std::this_thread::sleep_for(std::chrono::microseconds(5));
    return;
  }

  for (auto* mbuf : span<::rte_mbuf*>(mbufs.data(), num_frames)) {
    ::rte_vlan_strip(mbuf);
    notifier.get().on_new_frame(unique_rx_buffer(dpdk_rx_buffer_impl(mbuf)));
  }
  ofh_tracer << trace_event("ofh_dpdk_rx", dpdk_rx_tp);
}
