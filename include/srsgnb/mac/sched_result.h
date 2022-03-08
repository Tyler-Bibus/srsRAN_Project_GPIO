
#ifndef SRSGNB_SCHED_RESULT_H
#define SRSGNB_SCHED_RESULT_H

#include "sched_consts.h"
#include "srsgnb/adt/static_vector.h"
#include "srsgnb/ran/lcid.h"
#include "srsgnb/ran/pci.h"
#include "srsgnb/ran/rnti.h"
#include "srsgnb/ran/slot_point.h"
#include <cstddef>

namespace srsgnb {

/// Maximum grants per slot. Implementation-specific.
const size_t MAX_GRANTS = 16;
/// Maximum Logical channels per TB. Implementation-specific.
const size_t MAX_LC_GRANTS = 4;

struct pdcch_config {};

struct pdsch_config {};

struct dl_msg_lc_info {
  /// LCID {0..32}
  lcid_t lcid;
  /// Number of scheduled bytes for this specific logical channel. {0..65535}
  unsigned sched_bytes;
};

struct dl_msg_tb_info {
  /// List of allocated logical channels
  static_vector<dl_msg_lc_info, MAX_LC_GRANTS> lc_lst;
};

/// Dedicated DL Grant for UEs
struct dl_msg_alloc {
  rnti_t                                        crnti;
  static_vector<dl_msg_tb_info, MAX_NOF_LAYERS> tbs;
};

struct dl_sched_result {
  pci_t      cell_id;
  slot_point slot_value;

  /// Allocation of dedicated UE messages
  static_vector<dl_msg_alloc, MAX_GRANTS> ue_grants;
};

struct ul_sched_info {};

using ul_sched_result = static_vector<ul_sched_info, MAX_GRANTS>;

} // namespace srsgnb

#endif // SRSGNB_SCHED_RESULT_H
