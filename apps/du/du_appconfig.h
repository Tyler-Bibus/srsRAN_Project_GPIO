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

#include "apps/services/buffer_pool/buffer_pool_appconfig.h"
#include "apps/services/e2/e2_appconfig.h"
#include "apps/services/hal/hal_appconfig.h"
#include "apps/services/logger/logger_appconfig.h"
#include "apps/services/worker_manager/worker_manager_appconfig.h"
#include <optional>

namespace srsran {
namespace srs_du {

/// Configuration of the F1-C interface of the DU.
struct f1ap_appconfig {
  /// CU-CP F1-C address the DU will connect to.
  std::string cu_cp_address = "127.0.10.1";
  /// DU F1-C bind address.
  std::string bind_address = "127.0.10.2";
};

struct nru_appconfig {
  unsigned    pdu_queue_size = 2048;
  std::string bind_address   = "127.0.10.2";
  std::string ext_addr       = "auto"; // External address advertised by the F1-U interface
  float       pool_threshold = 0.9;
};

/// Metrics report configuration.
struct metrics_appconfig {
  std::string addr = "127.0.0.1";
  uint16_t    port = 55555;
};

} // namespace srs_du

/// DU application configuration.
struct du_appconfig {
  /// Default constructor to update the log filename.
  du_appconfig() { log_cfg.filename = "/tmp/du.log"; }
  /// DU multicell flag.
  bool du_multicell_enabled = false;
  /// Loggers configuration.
  logger_appconfig log_cfg;
  /// Metrics configuration.
  srs_du::metrics_appconfig metrics_cfg;
  /// E2 configuration.
  e2_appconfig e2_cfg;
  /// F1-C configuration.
  srs_du::f1ap_appconfig f1ap_cfg;
  /// F1-U configuration.
  srs_du::nru_appconfig nru_cfg;
  /// Buffer pool configuration.
  buffer_pool_appconfig buffer_pool_config;
  /// Expert configuration.
  expert_execution_appconfig expert_execution_cfg;
  /// HAL configuration.
  std::optional<hal_appconfig> hal_config;
};

} // namespace srsran
