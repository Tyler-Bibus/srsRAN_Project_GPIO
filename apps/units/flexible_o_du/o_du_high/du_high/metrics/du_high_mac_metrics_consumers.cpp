/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "du_high_mac_metrics_consumers.h"
#include "srsran/mac/mac_metrics.h"

using namespace srsran;

namespace {

DECLARE_METRIC("average_latency_us", metric_average_latency, double, "us");
DECLARE_METRIC("min_latency_us", metric_min_latency, double, "us");
DECLARE_METRIC("max_latency_us", metric_max_latency, double, "us");
DECLARE_METRIC("cpu_usage_percent", metric_cpu_usage, double, "");

DECLARE_METRIC_SET("cell_report",
                   mset_dl_cell,
                   metric_average_latency,
                   metric_min_latency,
                   metric_max_latency,
                   metric_cpu_usage);

DECLARE_METRIC_LIST("dl_cell_list", mlist_dl, std::vector<mset_dl_cell>);

/// Metrics root object.
DECLARE_METRIC_SET("L2", mset_l2, mlist_dl);
DECLARE_METRIC("timestamp", metric_timestamp_tag, double, "");

/// Metrics context.
using metric_context_t = srslog::build_context_type<metric_timestamp_tag, mset_l2>;

} // namespace

/// Returns the current time in seconds with ms precision since UNIX epoch.
static double get_time_stamp()
{
  auto tp = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(tp).count() * 1e-3;
}

void mac_metrics_consumer_json::handle_metric(const app_services::metrics_set& metric)
{
  const auto& mac_metrics = static_cast<const mac_metrics_impl&>(metric).get_metrics();

  metric_context_t ctx("JSON MAC Metrics");

  auto& dl_cells = ctx.get<mset_l2>().get<mlist_dl>();

  for (const auto& report : mac_metrics.dl.cells) {
    double metrics_period = (report.slot_duration * report.nof_slots).count();
    double cpu_usage      = 100.0 * static_cast<double>(report.wall_clock_latency.average.count()) / metrics_period;

    auto& output = dl_cells.emplace_back();
    output.write<metric_average_latency>(static_cast<double>(report.wall_clock_latency.average.count()) / 1000.0);
    output.write<metric_min_latency>(static_cast<double>(report.wall_clock_latency.min.count()) / 1000.0);
    output.write<metric_max_latency>(static_cast<double>(report.wall_clock_latency.max.count()) / 1000.0);
    output.write<metric_cpu_usage>(cpu_usage);
  }

  // Log the context.
  ctx.write<metric_timestamp_tag>(get_time_stamp());
  log_chan(ctx);
}
