/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "rrc_test_message_validators.h"

using namespace srsran;
using namespace asn1::rrc_nr;

#define TRUE_OR_RETURN(cond)                                                                                           \
  if (not(cond))                                                                                                       \
    return false;

bool srsran::test_helpers::is_valid_rrc_setup(const asn1::rrc_nr::dl_ccch_msg_s& msg)
{
  TRUE_OR_RETURN(msg.msg.type().value == asn1::rrc_nr::dl_ccch_msg_type_c::types_opts::c1);
  TRUE_OR_RETURN(msg.msg.c1().type().value == asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types_opts::rrc_setup);
  TRUE_OR_RETURN(msg.msg.c1().rrc_setup().crit_exts.type().value ==
                 asn1::rrc_nr::rrc_setup_s::crit_exts_c_::types_opts::rrc_setup);
  return true;
}

bool srsran::test_helpers::is_valid_rrc_setup(const byte_buffer& dl_ccch_msg)
{
  asn1::cbit_ref              bref{dl_ccch_msg};
  asn1::rrc_nr::dl_ccch_msg_s ccch;
  TRUE_OR_RETURN(ccch.unpack(bref) == asn1::SRSASN_SUCCESS);
  return is_valid_rrc_setup(ccch);
}

bool srsran::test_helpers::is_valid_rrc_reestablishment(const asn1::rrc_nr::dl_dcch_msg_s& msg)
{
  TRUE_OR_RETURN(msg.msg.type().value == asn1::rrc_nr::dl_dcch_msg_type_c::types_opts::c1);
  TRUE_OR_RETURN(msg.msg.c1().type().value == asn1::rrc_nr::dl_dcch_msg_type_c::c1_c_::types_opts::rrc_reest);
  TRUE_OR_RETURN(msg.msg.c1().rrc_reest().crit_exts.type().value ==
                 asn1::rrc_nr::rrc_reest_s::crit_exts_c_::types_opts::rrc_reest);
  return true;
}

bool srsran::test_helpers::is_valid_rrc_reestablishment(const byte_buffer& dl_dcch_msg)
{
  asn1::cbit_ref              bref{dl_dcch_msg};
  asn1::rrc_nr::dl_dcch_msg_s dcch;
  TRUE_OR_RETURN(dcch.unpack(bref) == asn1::SRSASN_SUCCESS);
  return is_valid_rrc_reestablishment(dcch);
}
