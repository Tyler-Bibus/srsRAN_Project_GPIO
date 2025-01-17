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

#include "srsran/adt/expected.h"
#include "srsran/adt/interval.h"
#include "srsran/ran/cyclic_prefix.h"
#include "srsran/ran/duplex_mode.h"
#include "srsran/ran/prach/rach_config_common.h"
#include <optional>

namespace srsran {

struct tdd_ul_dl_config_common;

namespace prach_helper {

/// Checks whether the PRACH config index is valid for a given band.
/// \return In case config is not supported, a string with an error message is provided.
error_type<std::string> prach_config_index_is_valid(uint8_t prach_cfg_idx, duplex_mode dplx_mode);

/// Checks whether the PRACH zero correlation zone is valid for a given PRACH config index and duplex mode.
/// \return In case config is not supported, a string with an error message is provided.
error_type<std::string>
zero_correlation_zone_is_valid(uint8_t zero_correlation_zone, uint8_t prach_cfg_idx, duplex_mode dplx_mode);

/// \brief Finds whether a PRACH config index fits in the provided TDD pattern.
/// \return In case of failure, returns the range of PRACH slots that did not fit in the TDD pattern. If the PRACH
/// configuration is invalid, an empty interval of slots is returned.
error_type<interval<uint8_t>>
prach_fits_in_tdd_pattern(subcarrier_spacing pusch_scs, uint8_t prach_cfg_idx, const tdd_ul_dl_config_common& tdd_cfg);

/// \brief Finds a PRACH configuration index that ensures that PRACH falls in an TDD UL slot.
std::optional<uint8_t> find_valid_prach_config_index(subcarrier_spacing             pusch_scs,
                                                     uint8_t                        zero_correlation_zone,
                                                     const tdd_ul_dl_config_common& tdd_cfg);

/// \brief Checks whether the nof. SSB per RACH occasion and nof. contention based preambles per SSB is valid.
/// \return In case config is not supported, a string with an error message is returned.
error_type<std::string> nof_ssb_per_ro_and_nof_cb_preambles_per_ssb_is_valid(float   nof_ssb_per_ro,
                                                                             uint8_t nof_cb_preambles_per_ssb);

} // namespace prach_helper
} // namespace srsran
