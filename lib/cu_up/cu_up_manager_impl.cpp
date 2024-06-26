/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "cu_up_manager_impl.h"
#include "srsran/support/async/execute_on.h"

using namespace srsran;
using namespace srs_cu_up;

void assert_cu_up_configuration_valid(const cu_up_configuration& cfg)
{
  srsran_assert(cfg.ue_exec_pool != nullptr, "Invalid CU-UP UE executor pool");
  srsran_assert(cfg.io_ul_executor != nullptr, "Invalid CU-UP IO UL executor");
  srsran_assert(cfg.e1ap.e1_conn_client != nullptr, "Invalid E1 connection client");
  srsran_assert(cfg.f1u_gateway != nullptr, "Invalid F1-U connector");
  srsran_assert(cfg.ngu_gw != nullptr, "Invalid N3 gateway");
  srsran_assert(cfg.gtpu_pcap != nullptr, "Invalid GTP-U pcap");
}

/// Helper functions
void fill_sec_as_config(security::sec_as_config& sec_as_config, const e1ap_security_info& sec_info);
void process_successful_pdu_resource_modification_outcome(
    slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_modified_item>& pdu_session_resource_modified_list,
    slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_failed_item>&
                                           pdu_session_resource_failed_to_modify_list,
    const pdu_session_modification_result& result,
    const srslog::basic_logger&            logger);
void process_successful_pdu_resource_setup_mod_outcome(
    slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_setup_modification_item>&
                                    pdu_session_resource_setup_list,
    const pdu_session_setup_result& result);

cu_up_manager_impl::cu_up_manager_impl(const cu_up_configuration& config_) : cfg(config_), main_ctrl_loop(128)
{
  assert_cu_up_configuration_valid(cfg);

  /// > Create UE manager
  ue_mng = std::make_unique<ue_manager>(cfg.net_cfg,
                                        cfg.n3_cfg,
                                        *e1ap,
                                        *cfg.timers,
                                        *cfg.f1u_gateway,
                                        gtpu_gw_adapter,
                                        *ngu_demux,
                                        *n3_teid_allocator,
                                        *f1u_teid_allocator,
                                        *cfg.ue_exec_pool,
                                        *cfg.gtpu_pcap,
                                        logger);
}

void cu_up_manager_impl::disconnect()
{
  gw_data_gtpu_demux_adapter.disconnect();
  gtpu_gw_adapter.disconnect();
  // e1ap_cu_up_ev_notifier.disconnect();
}

void cu_up_manager_impl::schedule_ue_async_task(ue_index_t ue_index, async_task<void> task)
{
  ue_mng->schedule_ue_async_task(ue_index, std::move(task));
}

async_task<e1ap_bearer_context_modification_response>
cu_up_manager_impl::handle_bearer_context_modification_request(const e1ap_bearer_context_modification_request& msg)
{
  ue_context* ue_ctxt = ue_mng->find_ue(msg.ue_index);
  if (ue_ctxt == nullptr) {
    logger.error("Could not find UE context");
    return {};
  }
  return execute_and_continue_on_blocking(
      ue_ctxt->ue_exec_mapper->ctrl_executor(), *cfg.ctrl_executor, [this, ue_ctxt, msg]() {
        return handle_bearer_context_modification_request_impl(*ue_ctxt, msg);
      });
}

e1ap_bearer_context_modification_response
cu_up_manager_impl::handle_bearer_context_modification_request_impl(ue_context& ue_ctxt,
                                                                    const e1ap_bearer_context_modification_request& msg)
{
  ue_ctxt.get_logger().log_debug("Handling BearerContextModificationRequest");

  e1ap_bearer_context_modification_response response = {};
  response.ue_index                                  = ue_ctxt.get_index();
  response.success                                   = true;

  bool new_ul_tnl_info_required = msg.new_ul_tnl_info_required == std::string("required");

  if (msg.security_info.has_value()) {
    security::sec_as_config security_info;
    fill_sec_as_config(security_info, msg.security_info.value());
    ue_ctxt.set_security_config(security_info);
  }

  if (msg.ng_ran_bearer_context_mod_request.has_value()) {
    // Traverse list of PDU sessions to be setup/modified
    for (const auto& pdu_session_item :
         msg.ng_ran_bearer_context_mod_request.value().pdu_session_res_to_setup_mod_list) {
      ue_ctxt.get_logger().log_debug("Setup/Modification of {}", pdu_session_item.pdu_session_id);
      pdu_session_setup_result session_result = ue_ctxt.setup_pdu_session(pdu_session_item);
      process_successful_pdu_resource_setup_mod_outcome(response.pdu_session_resource_setup_list, session_result);
      response.success &= session_result.success; // Update final result.
    }

    // Traverse list of PDU sessions to be modified.
    for (const auto& pdu_session_item : msg.ng_ran_bearer_context_mod_request.value().pdu_session_res_to_modify_list) {
      ue_ctxt.get_logger().log_debug("Modifying {}", pdu_session_item.pdu_session_id);
      pdu_session_modification_result session_result =
          ue_ctxt.modify_pdu_session(pdu_session_item, new_ul_tnl_info_required);
      process_successful_pdu_resource_modification_outcome(response.pdu_session_resource_modified_list,
                                                           response.pdu_session_resource_failed_to_modify_list,
                                                           session_result,
                                                           logger);
      ue_ctxt.get_logger().log_debug("Modification {}", session_result.success ? "successful" : "failed");

      response.success &= session_result.success; // Update final result.
    }

    // Traverse list of PDU sessions to be removed.
    for (const auto& pdu_session_item : msg.ng_ran_bearer_context_mod_request.value().pdu_session_res_to_rem_list) {
      ue_ctxt.get_logger().log_info("Removing {}", pdu_session_item);
      ue_ctxt.remove_pdu_session(pdu_session_item);
      // There is no IE to confirm successful removal.
    }
  } else {
    ue_ctxt.get_logger().log_warning("Ignoring empty Bearer Context Modification Request");
  }

  // 3. Create response
  response.success = true;
  return response;
}

void cu_up_manager_impl::handle_bearer_context_release_command(const e1ap_bearer_context_release_command& msg)
{
  ue_context* ue_ctxt = ue_mng->find_ue(msg.ue_index);
  if (ue_ctxt == nullptr) {
    logger.error("ue={}: Discarding E1 Bearer Context Release Command. UE context not found", msg.ue_index);
    return;
  }

  ue_ctxt->get_logger().log_debug("Received E1 Bearer Context Release Command");

  ue_mng->remove_ue(msg.ue_index);
}

/// Helper functions
void process_successful_pdu_resource_modification_outcome(
    slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_modified_item>& pdu_session_resource_modified_list,
    slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_failed_item>&
                                           pdu_session_resource_failed_to_modify_list,
    const pdu_session_modification_result& result,
    const srslog::basic_logger&            logger)
{
  if (result.success) {
    e1ap_pdu_session_resource_modified_item modified_item;
    modified_item.pdu_session_id = result.pdu_session_id;

    for (const auto& drb_setup_item : result.drb_setup_results) {
      logger.debug("Adding DRB setup result item. {}, success={}", drb_setup_item.drb_id, drb_setup_item.success);
      if (drb_setup_item.success) {
        e1ap_drb_setup_item_ng_ran res_drb_setup_item;
        res_drb_setup_item.drb_id = drb_setup_item.drb_id;

        e1ap_up_params_item up_param_item;
        up_param_item.up_tnl_info = drb_setup_item.gtp_tunnel;
        res_drb_setup_item.ul_up_transport_params.push_back(up_param_item);

        for (const auto& flow_item : drb_setup_item.qos_flow_results) {
          if (flow_item.success) {
            e1ap_qos_flow_item res_flow_setup_item;
            res_flow_setup_item.qos_flow_id = flow_item.qos_flow_id;
            res_drb_setup_item.flow_setup_list.emplace(flow_item.qos_flow_id, res_flow_setup_item);
          } else {
            e1ap_qos_flow_failed_item res_flow_failed_item;
            res_flow_failed_item.qos_flow_id = flow_item.qos_flow_id;
            res_flow_failed_item.cause       = flow_item.cause;
            res_drb_setup_item.flow_failed_list.emplace(flow_item.qos_flow_id, res_flow_failed_item);
          }
        }
        modified_item.drb_setup_list_ng_ran.emplace(drb_setup_item.drb_id, res_drb_setup_item);
      } else {
        e1ap_drb_failed_item_ng_ran asn1_drb_failed_item;
        asn1_drb_failed_item.drb_id = drb_setup_item.drb_id;
        asn1_drb_failed_item.cause  = drb_setup_item.cause;

        modified_item.drb_failed_list_ng_ran.emplace(drb_setup_item.drb_id, asn1_drb_failed_item);
      }
    }
    for (const auto& drb_modified_item : result.drb_modification_results) {
      logger.debug(
          "Adding DRB modified result item. {}, success={}", drb_modified_item.drb_id, drb_modified_item.success);
      e1ap_drb_modified_item_ng_ran e1ap_mod_item;
      e1ap_mod_item.drb_id = drb_modified_item.drb_id;

      e1ap_up_params_item up_param_item;
      up_param_item.up_tnl_info = drb_modified_item.gtp_tunnel;
      e1ap_mod_item.ul_up_transport_params.push_back(up_param_item);
      modified_item.drb_modified_list_ng_ran.emplace(e1ap_mod_item.drb_id, e1ap_mod_item);
    }

    pdu_session_resource_modified_list.emplace(modified_item.pdu_session_id, modified_item);
  } else {
    e1ap_pdu_session_resource_failed_item failed_item;
    failed_item.pdu_session_id = result.pdu_session_id;
    failed_item.cause          = e1ap_cause_radio_network_t::unspecified;
    pdu_session_resource_failed_to_modify_list.emplace(failed_item.pdu_session_id, failed_item);
  }
}

void process_successful_pdu_resource_setup_mod_outcome(
    slotted_id_vector<pdu_session_id_t, e1ap_pdu_session_resource_setup_modification_item>&
                                    pdu_session_resource_setup_list,
    const pdu_session_setup_result& result)
{
  if (result.success) {
    e1ap_pdu_session_resource_setup_modification_item res_setup_item;
    res_setup_item.pdu_session_id    = result.pdu_session_id;
    res_setup_item.ng_dl_up_tnl_info = result.gtp_tunnel;
    res_setup_item.security_result   = result.security_result;
    for (const auto& drb_setup_item : result.drb_setup_results) {
      if (drb_setup_item.success) {
        e1ap_drb_setup_item_ng_ran res_drb_setup_item;
        res_drb_setup_item.drb_id = drb_setup_item.drb_id;

        e1ap_up_params_item up_param_item;
        up_param_item.up_tnl_info = drb_setup_item.gtp_tunnel;
        res_drb_setup_item.ul_up_transport_params.push_back(up_param_item);

        for (const auto& flow_item : drb_setup_item.qos_flow_results) {
          if (flow_item.success) {
            e1ap_qos_flow_item res_flow_setup_item;
            res_flow_setup_item.qos_flow_id = flow_item.qos_flow_id;
            res_drb_setup_item.flow_setup_list.emplace(flow_item.qos_flow_id, res_flow_setup_item);
          } else {
            e1ap_qos_flow_failed_item res_flow_failed_item;
            res_flow_failed_item.qos_flow_id = flow_item.qos_flow_id;
            res_flow_failed_item.cause       = flow_item.cause;
            res_drb_setup_item.flow_failed_list.emplace(flow_item.qos_flow_id, res_flow_failed_item);
          }
        }
        res_setup_item.drb_setup_list_ng_ran.emplace(drb_setup_item.drb_id, res_drb_setup_item);
      } else {
        e1ap_drb_failed_item_ng_ran asn1_drb_failed_item;
        asn1_drb_failed_item.drb_id = drb_setup_item.drb_id;
        asn1_drb_failed_item.cause  = drb_setup_item.cause;

        res_setup_item.drb_failed_list_ng_ran.emplace(drb_setup_item.drb_id, asn1_drb_failed_item);
      }
    }
    pdu_session_resource_setup_list.emplace(result.pdu_session_id, res_setup_item);
  }
}
