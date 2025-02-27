/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "e1ap_asn1_converters.h"
#include "srsran/asn1/e1ap/e1ap_pdu_contents.h"
#include "srsran/ran/bcd_helper.h"
#include "srsran/ran/qos/qos_prio_level.h"

namespace srsran {

inline void fill_e1ap_cu_up_e1_setup_request(cu_up_e1_setup_request&                         req,
                                             const asn1::e1ap::gnb_cu_up_e1_setup_request_s& asn1_req)
{
  req.gnb_cu_up_id = asn1_req->gnb_cu_up_id;

  if (asn1_req->gnb_cu_up_name_present) {
    req.gnb_cu_up_name = asn1_req->gnb_cu_up_name.to_string();
  }

  req.cn_support = static_cast<cu_up_cn_support_t>(asn1_req->cn_support.value);

  for (const auto& asn1_plmn_item : asn1_req->supported_plmns) {
    supported_plmns_item_t plmn;
    plmn.plmn_id = asn1_plmn_item.plmn_id.to_string();

    for (const auto& asn1_slice_support_item : asn1_plmn_item.slice_support_list) {
      slice_support_item_t slice_support;
      slice_support.s_nssai.sst = slice_service_type{(uint8_t)asn1_slice_support_item.snssai.sst.to_number()};

      if (asn1_slice_support_item.snssai.sd_present) {
        slice_support.s_nssai.sd = slice_differentiator::create(asn1_slice_support_item.snssai.sd.to_number()).value();
      }

      plmn.slice_support_list.push_back(slice_support);
    }

    for (const auto& asn1_nr_cgi_support_item : asn1_plmn_item.nr_cgi_support_list) {
      nr_cgi_support_item_t nr_cgi_support;
      nr_cgi_support.nr_cgi = e1ap_asn1_to_cgi(asn1_nr_cgi_support_item.nr_cgi);

      plmn.nr_cgi_support_list.push_back(nr_cgi_support);
    }

    if (asn1_plmn_item.qos_params_support_list_present) {
      // We only support ng ran qos support list
      for (const auto& asn1_qos_support_item : asn1_plmn_item.qos_params_support_list.ng_ran_qos_support_list) {
        ng_ran_qos_support_item_t qos_support_item;

        qos_support_item.non_dyn_5qi_desc.five_qi =
            uint_to_five_qi(asn1_qos_support_item.non_dyn_5qi_descriptor.five_qi);

        if (asn1_qos_support_item.non_dyn_5qi_descriptor.qos_prio_level_present) {
          qos_support_item.non_dyn_5qi_desc.qos_prio_level =
              qos_prio_level_t{asn1_qos_support_item.non_dyn_5qi_descriptor.qos_prio_level};
        }
        if (asn1_qos_support_item.non_dyn_5qi_descriptor.averaging_win_present) {
          qos_support_item.non_dyn_5qi_desc.averaging_win = asn1_qos_support_item.non_dyn_5qi_descriptor.averaging_win;
        }
        if (asn1_qos_support_item.non_dyn_5qi_descriptor.max_data_burst_volume_present) {
          qos_support_item.non_dyn_5qi_desc.max_data_burst_volume =
              asn1_qos_support_item.non_dyn_5qi_descriptor.max_data_burst_volume;
        }

        plmn.ng_ran_qos_support_list.push_back(qos_support_item);
      }
    }

    req.supported_plmns.push_back(plmn);
  }

  if (asn1_req->gnb_cu_up_capacity_present) {
    req.gnb_cu_up_capacity = asn1_req->gnb_cu_up_capacity;
  }
}

} // namespace srsran
