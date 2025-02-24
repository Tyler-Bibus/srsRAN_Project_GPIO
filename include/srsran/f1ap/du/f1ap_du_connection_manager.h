/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/ran/carrier_configuration.h"
#include "srsran/ran/duplex_mode.h"
#include "srsran/ran/gnb_du_id.h"
#include "srsran/ran/nr_cgi.h"
#include "srsran/ran/pci.h"
#include "srsran/ran/s_nssai.h"
#include "srsran/ran/subcarrier_spacing.h"
#include "srsran/ran/tac.h"
#include "srsran/support/async/async_task.h"
#include <optional>

namespace srsran {
namespace srs_du {

/// System Information Update from the gNB-DU.
struct gnb_du_sys_info {
  byte_buffer packed_mib;
  byte_buffer packed_sib1;
};

/// Information of served cell being added/modified.
struct du_served_cell_info {
  nr_cell_global_id_t                  nr_cgi;
  pci_t                                pci;
  tac_t                                tac;
  duplex_mode                          duplx_mode;
  subcarrier_spacing                   scs_common;
  carrier_configuration                dl_carrier;
  std::optional<carrier_configuration> ul_carrier;
  byte_buffer                          packed_meas_time_cfg;
};

/// \brief Served cell configuration that will be passed to CU-CP.
struct f1_cell_setup_params {
  du_served_cell_info    cell_info;
  gnb_du_sys_info        du_sys_info;
  std::vector<s_nssai_t> slices;
};

/// \brief Message that initiates a F1 Setup procedure.
struct f1_setup_request_message {
  gnb_du_id_t                       gnb_du_id;
  std::string                       gnb_du_name;
  uint8_t                           rrc_version;
  std::vector<f1_cell_setup_params> served_cells;
  unsigned                          max_setup_retries = 5;
};

/// Outcome of the F1 Setup procedure.
struct f1_setup_success {
  struct cell_to_activate {
    nr_cell_global_id_t cgi;
  };
  std::vector<cell_to_activate> cells_to_activate;
};
struct f1_setup_failure {
  /// Possible result outcomes for F1 Setup failure.
  enum class result_code { timeout, proc_failure, invalid_response, f1_setup_failure };

  /// Result outcome for F1 Setup failure.
  result_code result;
  /// Cause provided by CU-CP in case of F1 Setup Failure.
  std::string f1_setup_failure_cause;
};
using f1_setup_result = expected<f1_setup_success, f1_setup_failure>;

/// Cell whose parameters need to be modified in the DU.
struct f1ap_cell_to_be_modified {
  /// New served Cell Information.
  du_served_cell_info cell_info;
  /// New System Information.
  std::optional<gnb_du_sys_info> du_sys_info;
};

/// gNB-DU initiated Config Update as per TS 38.473, Section 8.2.4.
struct gnbdu_config_update_request {
  std::vector<f1ap_cell_to_be_modified> cells_to_mod;
};

struct gnbdu_config_update_response {
  bool result;
};

/// Handle F1AP interface management procedures as defined in TS 38.473 section 8.2.
class f1ap_connection_manager
{
public:
  virtual ~f1ap_connection_manager() = default;

  /// \brief Connect the DU to CU-CP via F1-C interface.
  [[nodiscard]] virtual bool connect_to_cu_cp() = 0;

  /// \brief Initiates the F1 Setup procedure as per TS 38.473, Section 8.2.3.
  /// \param[in] request The F1SetupRequest message to transmit.
  /// \return Returns a f1_setup_response_message struct with the success member set to 'true' in case of a
  /// successful outcome, 'false' otherwise. \remark The DU transmits the F1SetupRequest as per TS 38.473 section 8.2.3
  /// and awaits the response. If a F1SetupFailure is received the F1AP will handle the failure.
  virtual async_task<f1_setup_result> handle_f1_setup_request(const f1_setup_request_message& request) = 0;

  /// \brief Launches the F1 Removal procedure as per TS 38.473, Section 8.2.8.
  virtual async_task<void> handle_f1_removal_request() = 0;

  /// \brief Initiates F1AP gNB-DU config update procedure as per TS 38.473, Section 8.2.4.
  virtual async_task<gnbdu_config_update_response>
  handle_du_config_update(const gnbdu_config_update_request& request) = 0;
};

} // namespace srs_du
} // namespace srsran
