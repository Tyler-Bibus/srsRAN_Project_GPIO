/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "resource_usage.h"
#include "srsran/support/compiler.h"
#include <chrono>
#include <string>
#include <vector>

namespace srslog {

class log_channel;

} // namespace srslog

namespace srsran {

/// \brief Trace Event clock type.
///
/// \remark We use high_resolution_clock instead of steady_clock for time stamps to be aligned with logging.
using trace_clock    = std::chrono::high_resolution_clock;
using trace_point    = trace_clock::time_point;
using trace_duration = std::chrono::microseconds;

/// Trace point value when tracing is disabled.
constexpr static trace_point null_trace_point = {};

/// Resource usaged value when tracing is disabled.
constexpr static resource_usage::snapshot null_rusage_snapshot{0, 0};

/// Open a file to write trace events to.
void open_trace_file(std::string_view trace_file_name = "/tmp/srsran_trace.json");

/// Close the trace file. This function is called automatically when the program exits.
void close_trace_file();

/// Check if the trace file is open.
bool is_trace_file_open();

/// \brief Trace event used for events with defined name, starting point and duration.
/// \remark The creation of this type should be trivial so that compiler optimizes it out for null tracers.
struct trace_event {
  const char* name;
  trace_point start_tp;

  SRSRAN_FORCE_INLINE constexpr trace_event(const char* name_, trace_point start_tp_) : name(name_), start_tp(start_tp_)
  {
  }
};

struct trace_thres_event {
  const char*    name;
  trace_point    start_tp;
  trace_duration thres;

  SRSRAN_FORCE_INLINE constexpr trace_thres_event(const char* name_, trace_point start_tp_, trace_duration thres_) :
    name(name_), start_tp(start_tp_), thres(thres_)
  {
  }
};

/// \brief Trace event type with defined name, starting point but no duration.
/// \remark The creation of this type should be trivial so that compiler optimizes it out for null tracers.
struct instant_trace_event {
  enum class cpu_scope { global, process, thread };

  const char* name;
  cpu_scope   scope;

  SRSRAN_FORCE_INLINE constexpr instant_trace_event(const char* name_, cpu_scope scope_) : name(name_), scope(scope_) {}
};

/// \brief Event like \c trace_thres_event but that also captures resource usage.
struct rusage_thres_trace_event {
  const char*              name;
  trace_duration           thres;
  trace_point              start_tp;
  resource_usage::snapshot rusg_capture;

  SRSRAN_FORCE_INLINE constexpr rusage_thres_trace_event(const char*              name_,
                                                         trace_duration           thres_,
                                                         trace_point              start_tp_,
                                                         resource_usage::snapshot start_rusage_) :
    name(name_), thres(thres_), start_tp(start_tp_), rusg_capture(start_rusage_)
  {
  }
};

namespace detail {

/// \brief Tracer that does not write any events. The compiler should eliminate all calls to this tracer when
/// optimizations are enabled.
class null_event_tracer
{
public:
  static constexpr trace_point    now() { return null_trace_point; }
  static resource_usage::snapshot rusage_now() { return null_rusage_snapshot; }

  void operator<<(const trace_event& event) const {}

  void operator<<(const trace_thres_event& event) const {}

  void operator<<(const instant_trace_event& event) const {}

  void operator<<(const rusage_thres_trace_event& event) const {}
};

} // namespace detail

/// \brief Class that writes trace events to a dedicated trace file.
template <bool Enabled = true>
class file_event_tracer
{
public:
  static trace_point              now() { return trace_clock::now(); }
  static resource_usage::snapshot rusage_now() { return resource_usage::now().value_or(null_rusage_snapshot); }

  void operator<<(const trace_event& event) const;

  void operator<<(const trace_thres_event& event) const;

  void operator<<(const instant_trace_event& event) const;

  void operator<<(const rusage_thres_trace_event& event) const;
};

/// Specialization of file_event_tracer that does not write any events.
template <>
class file_event_tracer<false> : public detail::null_event_tracer
{
};

/// Class that repurposes a log channel to write trace events.
template <bool Enabled = true>
class logger_event_tracer
{
public:
  explicit logger_event_tracer(srslog::log_channel& log_ch_) : log_ch(log_ch_) {}

  static trace_point              now() { return trace_clock::now(); }
  static resource_usage::snapshot rusage_now() { return resource_usage::now().value_or(null_rusage_snapshot); }

  void operator<<(const trace_event& event) const;

  void operator<<(const trace_thres_event& event) const;

  void operator<<(const instant_trace_event& event) const;

  void operator<<(const rusage_thres_trace_event& event) const;

private:
  srslog::log_channel& log_ch;
};

template <>
class logger_event_tracer<false> : public detail::null_event_tracer
{
};

/// Class that writes trace events to a vector of strings for testing purposes.
class test_event_tracer
{
public:
  static trace_point              now() { return trace_clock::now(); }
  static resource_usage::snapshot rusage_now() { return resource_usage::now().value_or(null_rusage_snapshot); }

  void operator<<(const trace_event& event);

  void operator<<(const trace_thres_event& event);

  void operator<<(const instant_trace_event& event);

  void operator<<(const rusage_thres_trace_event& event);

  std::vector<std::string> pop_last_events() { return std::move(last_events); }

  void set_log_style_format(bool log_style) { is_log_stype = log_style; }

private:
  std::vector<std::string> last_events;
  bool                     is_log_stype = false;
};

} // namespace srsran
