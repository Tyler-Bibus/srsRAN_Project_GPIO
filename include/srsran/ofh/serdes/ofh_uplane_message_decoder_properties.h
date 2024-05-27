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

#include "srsran/adt/complex.h"
#include "srsran/adt/optional.h"
#include "srsran/adt/static_vector.h"
#include "srsran/ofh/serdes/ofh_uplane_message_properties.h"
#include "srsran/ran/resource_block.h"

namespace srsran {
namespace ofh {

/// Maximum number of supported sections.
static constexpr unsigned MAX_NOF_SUPPORTED_SECTIONS = 1U;

/// Open Fronthaul User-Plane section parameters.
struct uplane_section_params {
  /// Section identifier.
  unsigned section_id;
  /// Resource block indicator.
  bool is_every_rb_used;
  /// Symbol number increment command.
  bool use_current_symbol_number;
  /// Start PRB.
  unsigned start_prb;
  /// Number of PRBs (though a value of 0 signals more than 255 PRBs in the OFH specification, this field always
  /// contains the real amount of PRBs).
  unsigned nof_prbs;
  /// User data compression header.
  ru_compression_params ud_comp_hdr;
  /// User data compression length.
  std::optional<unsigned> ud_comp_len;
  /// User data compression parameter.
  /// \note For simplicity, all the PRBs use the same compression parameters.
  std::optional<unsigned> ud_comp_param;
  /// IQ samples for the number of PRBs indicated by \c nof_prbs.
  static_vector<cf_t, MAX_NOF_PRBS * NOF_SUBCARRIERS_PER_RB> iq_samples;
};

/// Open Fronthaul User-Plane message decoder results.
struct uplane_message_decoder_results {
  /// Maximum number of sections supported by this result.
  static constexpr unsigned RESULTS_MAX_NOF_SUPPORTED_SECTIONS = 2U;

  /// Open Fronthaul User-Plane message parameters.
  uplane_message_params params;
  /// User-Plane message sections.
  static_vector<uplane_section_params, RESULTS_MAX_NOF_SUPPORTED_SECTIONS> sections;
};

} // namespace ofh
} // namespace srsran
