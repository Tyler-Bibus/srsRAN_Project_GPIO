/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "mock_du.h"
#include "lib/f1ap/common/f1ap_asn1_utils.h"
#include "tests/test_doubles/f1ap/f1ap_test_messages.h"
#include "srsran/adt/mutexed_mpmc_queue.h"
#include "srsran/cu_cp/cu_cp_f1c_handler.h"
#include "srsran/f1ap/common/f1ap_message.h"
#include "srsran/support/error_handling.h"
#include <unordered_map>

using namespace srsran;
using namespace srs_cu_cp;

namespace {

/// \brief Mock class for the interface between DU and CU-CP that accounts for the fact that the CU-CP may push PDUs
/// from different threads.
class synchronized_mock_du final : public mock_du
{
public:
  synchronized_mock_du(mock_du_params params) : cu_cp_f1c(params.cu_cp)
  {
    tx_pdu_notifier = cu_cp_f1c.handle_new_du_connection(std::make_unique<rx_pdu_notifier>(*this));
  }

  bool connected() const { return tx_pdu_notifier != nullptr; }

  void push_ul_pdu(const f1ap_message& msg) override
  {
    report_fatal_error_if_not(tx_pdu_notifier != nullptr, "TNL connection is not established");

    if (msg.pdu.type().value == asn1::f1ap::f1ap_pdu_c::types_opts::init_msg) {
      if (msg.pdu.init_msg().value.type().value ==
          asn1::f1ap::f1ap_elem_procs_o::init_msg_c::types_opts::init_ul_rrc_msg_transfer) {
        const asn1::f1ap::init_ul_rrc_msg_transfer_s& rrcmsg = msg.pdu.init_msg().value.init_ul_rrc_msg_transfer();

        gnb_du_ue_f1ap_id_t du_ue_id = int_to_gnb_du_ue_f1ap_id(rrcmsg->gnb_du_ue_f1ap_id);
        report_fatal_error_if_not(
            ue_contexts.insert(std::make_pair(du_ue_id, ue_context{du_ue_id, nullopt, {0, 0, 0}})).second,
            "DU UE ID already exists");
      } else if (msg.pdu.init_msg().value.type().value ==
                 asn1::f1ap::f1ap_elem_procs_o::init_msg_c::types_opts::ul_rrc_msg_transfer) {
        const asn1::f1ap::ul_rrc_msg_transfer_s& rrcmsg   = msg.pdu.init_msg().value.ul_rrc_msg_transfer();
        gnb_du_ue_f1ap_id_t                      du_ue_id = int_to_gnb_du_ue_f1ap_id(rrcmsg->gnb_du_ue_f1ap_id);
        auto&                                    ue_ctx   = ue_contexts.at(du_ue_id);

        // update UL PDCP SN of the SRB.
        ue_ctx.srb_ul_pdcp_sn[rrcmsg->srb_id]++;
      }
    }

    tx_pdu_notifier->on_new_message(msg);
  }

  void push_rrc_ul_dcch_message(gnb_du_ue_f1ap_id_t du_ue_id, srb_id_t srb_id, byte_buffer ul_dcch_msg) override
  {
    // Fetch next UL PDCP SN for this UE and SRB.
    auto&    ue_ctx  = ue_contexts.at(du_ue_id);
    uint32_t pdcp_sn = ue_ctx.srb_ul_pdcp_sn.at((unsigned)srb_id);

    push_ul_pdu(test_helpers::create_ul_rrc_message_transfer(
        ue_ctx.du_ue_id, *ue_ctx.cu_ue_id, srb_id, pdcp_sn, std::move(ul_dcch_msg)));
  }

  bool try_pop_dl_pdu(f1ap_message& msg) override
  {
    if (not rx_pdus.try_pop(msg)) {
      return false;
    }

    // Update gNB-CU-UE-F1AP-ID in the UE context.
    optional<gnb_du_ue_f1ap_id_t> gnb_du_ue_f1ap_id = srsran::get_gnb_du_ue_f1ap_id(msg.pdu);
    optional<gnb_cu_ue_f1ap_id_t> gnb_cu_ue_f1ap_id = srsran::get_gnb_cu_ue_f1ap_id(msg.pdu);
    if (gnb_du_ue_f1ap_id.has_value()) {
      auto& ue_ctx = ue_contexts.at(gnb_du_ue_f1ap_id.value());
      if (gnb_cu_ue_f1ap_id.has_value()) {
        ue_ctx.cu_ue_id = gnb_cu_ue_f1ap_id;
      }
    }

    return true;
  }

private:
  class rx_pdu_notifier final : public f1ap_message_notifier
  {
  public:
    rx_pdu_notifier(synchronized_mock_du& parent_) : parent(parent_) {}

    void on_new_message(const f1ap_message& msg) override { parent.handle_rx_pdu(msg); }

  private:
    synchronized_mock_du& parent;
  };

  struct ue_context {
    gnb_du_ue_f1ap_id_t           du_ue_id;
    optional<gnb_cu_ue_f1ap_id_t> cu_ue_id;
    std::array<uint32_t, 3>       srb_ul_pdcp_sn{0, 0, 0};
  };

  void handle_rx_pdu(const f1ap_message& msg)
  {
    report_fatal_error_if_not(rx_pdus.push_blocking(msg), "queue is full");
  }

  cu_cp_f1c_handler&                                  cu_cp_f1c;
  std::unique_ptr<f1ap_message_notifier>              tx_pdu_notifier;
  std::unordered_map<gnb_du_ue_f1ap_id_t, ue_context> ue_contexts;

  concurrent_queue<f1ap_message,
                   srsran::concurrent_queue_policy::locking_mpmc,
                   srsran::concurrent_queue_wait_policy::condition_variable>
      rx_pdus{1024};
};

} // namespace

std::unique_ptr<mock_du> srsran::srs_cu_cp::create_mock_du(mock_du_params params)
{
  auto du = std::make_unique<synchronized_mock_du>(params);
  if (not du->connected()) {
    return nullptr;
  }
  return du;
}
