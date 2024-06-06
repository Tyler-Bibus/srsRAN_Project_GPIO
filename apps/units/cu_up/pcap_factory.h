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

#include "apps/services/worker_manager_worker_getter.h"
#include "apps/units/cu_up/cu_up_unit_pcap_config.h"
#include "srsran/pcap/dlt_pcap.h"

namespace srsran::modules::cu_up {

/// Types of PCAPs in the CU-UP
enum class pcap_type { E1AP, N3, F1U, last };

/// Converts the given PCAP type to an unsigned.
inline unsigned to_value(pcap_type value)
{
  return static_cast<unsigned>(value);
}

/// Creates the DLT PCAPs of the CU-UP.
inline std::vector<std::unique_ptr<dlt_pcap>> create_dlt_pcaps(const cu_up_unit_pcap_config&   pcap_cfg,
                                                               worker_manager_executor_getter* exec_getter)
{
  std::vector<std::unique_ptr<dlt_pcap>> pcaps(to_value(pcap_type::last));

  pcaps[to_value(pcap_type::E1AP)] =
      pcap_cfg.e1ap.enabled ? create_e1ap_pcap(pcap_cfg.e1ap.filename, exec_getter->get_executor("pcap_exec"))
                            : create_null_dlt_pcap();

  pcaps[to_value(pcap_type::N3)] =
      pcap_cfg.n3.enabled ? create_gtpu_pcap(pcap_cfg.n3.filename, exec_getter->get_executor("n3_pcap_exec"))
                          : create_null_dlt_pcap();

  pcaps[to_value(pcap_type::F1U)] =
      pcap_cfg.f1u.enabled ? create_gtpu_pcap(pcap_cfg.f1u.filename, exec_getter->get_executor("f1u_pcap_exec"))
                           : create_null_dlt_pcap();

  return pcaps;
}

} // namespace srsran::modules::cu_up
