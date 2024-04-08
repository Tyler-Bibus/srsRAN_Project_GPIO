/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "f1ap_test_message_validators.h"
#include "../lib/f1ap/common/asn1_helpers.h"
#include "../tests/test_doubles/rrc/rrc_test_message_validators.h"
#include "srsran/asn1/f1ap/common.h"
#include "srsran/asn1/f1ap/f1ap_pdu_contents.h"
#include "srsran/asn1/rrc_nr/dl_dcch_msg_ies.h"
#include "srsran/f1ap/common/f1ap_message.h"

using namespace srsran;

#define TRUE_OR_RETURN(cond)                                                                                           \
  if (not(cond))                                                                                                       \
    return false;

bool srsran::test_helpers::is_init_ul_rrc_msg_transfer_valid(const f1ap_message&           msg,
                                                             rnti_t                        rnti,
                                                             optional<nr_cell_global_id_t> nci)
{
  TRUE_OR_RETURN(msg.pdu.type() == asn1::f1ap::f1ap_pdu_c::types_opts::init_msg);
  TRUE_OR_RETURN(msg.pdu.init_msg().proc_code == ASN1_F1AP_ID_INIT_UL_RRC_MSG_TRANSFER);
  const asn1::f1ap::init_ul_rrc_msg_transfer_s& rrcmsg = msg.pdu.init_msg().value.init_ul_rrc_msg_transfer();

  TRUE_OR_RETURN(to_rnti(rrcmsg->c_rnti) == rnti);

  if (nci.has_value() and cgi_from_asn1(rrcmsg->nr_cgi) != nci) {
    return false;
  }

  return true;
}

bool srsran::test_helpers::is_valid_dl_rrc_message_transfer_with_msg4(const f1ap_message& msg)
{
  TRUE_OR_RETURN(msg.pdu.type() == asn1::f1ap::f1ap_pdu_c::types_opts::init_msg);
  TRUE_OR_RETURN(msg.pdu.init_msg().proc_code == ASN1_F1AP_ID_DL_RRC_MSG_TRANSFER);

  const asn1::f1ap::dl_rrc_msg_transfer_s& rrcmsg = msg.pdu.init_msg().value.dl_rrc_msg_transfer();

  TRUE_OR_RETURN(rrcmsg->srb_id <= srb_id_to_uint(srb_id_t::srb1));

  if (int_to_srb_id(rrcmsg->srb_id) == srb_id_t::srb0) {
    // RRC Setup.
    TRUE_OR_RETURN(not rrcmsg->old_gnb_du_ue_f1ap_id_present);
    TRUE_OR_RETURN(test_helpers::is_valid_rrc_setup(rrcmsg->rrc_container));

  } else if (int_to_srb_id(rrcmsg->srb_id) == srb_id_t::srb1) {
    // RRC Reestablishment.
    TRUE_OR_RETURN(rrcmsg->old_gnb_du_ue_f1ap_id_present);
    TRUE_OR_RETURN(rrcmsg->old_gnb_du_ue_f1ap_id != rrcmsg->old_gnb_du_ue_f1ap_id);

    // Remove PDCP header
    byte_buffer dl_dcch = rrcmsg->rrc_container.deep_copy().value();
    dl_dcch.trim_head(2);
    TRUE_OR_RETURN(test_helpers::is_valid_rrc_reestablishment(dl_dcch));
  }

  return true;
}

bool srsran::test_helpers::is_ul_rrc_msg_transfer_valid(const f1ap_message& msg, srb_id_t srb_id)
{
  if (not(msg.pdu.type() == asn1::f1ap::f1ap_pdu_c::types_opts::init_msg and
          msg.pdu.init_msg().proc_code == ASN1_F1AP_ID_UL_RRC_MSG_TRANSFER)) {
    return false;
  }
  const asn1::f1ap::ul_rrc_msg_transfer_s& rrcmsg = msg.pdu.init_msg().value.ul_rrc_msg_transfer();
  if (rrcmsg->srb_id != srb_id_to_uint(srb_id) or rrcmsg->rrc_container.empty()) {
    return false;
  }
  return true;
}

bool srsran::test_helpers::is_ue_context_setup_response_valid(const f1ap_message& msg)
{
  if (not(msg.pdu.type() == asn1::f1ap::f1ap_pdu_c::types_opts::successful_outcome and
          msg.pdu.successful_outcome().proc_code == ASN1_F1AP_ID_UE_CONTEXT_SETUP)) {
    return false;
  }
  const asn1::f1ap::ue_context_setup_resp_s& resp = msg.pdu.successful_outcome().value.ue_context_setup_resp();
  if (not resp->drbs_setup_list_present) {
    return false;
  }
  return true;
}
