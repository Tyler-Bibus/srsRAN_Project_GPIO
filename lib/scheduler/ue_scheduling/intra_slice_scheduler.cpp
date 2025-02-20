/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "intra_slice_scheduler.h"

using namespace srsran;

intra_slice_scheduler::intra_slice_scheduler(const scheduler_ue_expert_config& expert_cfg_,
                                             ue_repository&                    ues_,
                                             srslog::basic_logger&             logger_) :
  expert_cfg(expert_cfg_), ues(ues_), logger(logger_), ue_alloc(expert_cfg_, ues_, logger_)
{
  dl_newtx_candidates.reserve(MAX_NOF_DU_UES);
}

void intra_slice_scheduler::slot_indication(slot_point sl_tx)
{
  last_sl_tx        = sl_tx;
  dl_attempts_count = 0;
}

void intra_slice_scheduler::dl_sched(slot_point                    pdcch_slot,
                                     du_cell_index_t               cell_index,
                                     const dl_ran_slice_candidate& candidate,
                                     scheduler_policy&             policy)
{
  const slice_ue_repository& slice_ues  = candidate.get_slice_ues();
  slot_point                 pdsch_slot = candidate.get_slot_tx();

  // Determine max number of UE grants that can be scheduled in this slot.
  unsigned pdschs_to_alloc = max_pdschs_to_alloc(pdcch_slot, pdsch_slot, cell_index);
  if (pdschs_to_alloc == 0) {
    return;
  }

  // Schedule reTxs.
  unsigned nof_retxs_alloc = schedule_retx_candidates(cell_index, candidate, pdschs_to_alloc);
  pdschs_to_alloc -= std::min(pdschs_to_alloc, nof_retxs_alloc);
  if (pdschs_to_alloc == 0) {
    return;
  }

  // Build list of UE candidates.
  for (const slice_ue& u : slice_ues) {
    auto ue_candidate = create_newtx_dl_candidate(pdcch_slot, pdsch_slot, cell_index, u);
    if (ue_candidate.has_value()) {
      dl_newtx_candidates.push_back(ue_candidate.value());
    }
  }

  // Compute priorities using the provided policy.
  // TODO

  // Sort candidates by priority in descending order.
  std::sort(dl_newtx_candidates.begin(), dl_newtx_candidates.end(), [](const auto& a, const auto& b) {
    return a.priority > b.priority;
  });

  // Remove candidates with forbid priority.
  auto rit = std::find_if(dl_newtx_candidates.rbegin(), dl_newtx_candidates.rend(), [](const auto& cand) {
    return cand.priority != forbid_sched_priority;
  });
  dl_newtx_candidates.erase(rit.base(), dl_newtx_candidates.end());
  if (dl_newtx_candidates.empty()) {
    return;
  }

  // Allocate UE newTx grants.
  schedule_newtx_candidates(cell_index, candidate, pdschs_to_alloc);
}

static std::pair<unsigned, unsigned>
get_max_grants_and_dl_rb_grant_size(span<const ue_pdsch_newtx_candidate> ue_candidates,
                                    const cell_resource_allocator&       cell_alloc,
                                    const dl_ran_slice_candidate&        slice,
                                    unsigned                             max_ue_grants_to_alloc)
{
  // (Implementation-defined) We use the same searchSpace config to determine the number of RBs available.
  const ue_cell_configuration& ue_cfg  = ue_candidates[0].ue_cc->cfg();
  const search_space_id        ss_id   = ue_cfg.init_bwp().dl_ded.value()->pdcch_cfg->search_spaces.back().get_id();
  const auto*                  ss_info = ue_cfg.find_search_space(ss_id);
  if (ss_info == nullptr) {
    return std::make_pair(0, 0);
  }

  // Reduce number of UEs to alloc based on the number of UE candidates.
  max_ue_grants_to_alloc =
      std::min(max_ue_grants_to_alloc, std::max(std::min((unsigned)ue_candidates.size() / 4U, 1U), 8U));

  // > Compute maximum nof. PDCCH candidates allowed for each direction.
  // [Implementation-defined]
  // - Assume aggregation level 2 while computing nof. candidates that can be fit in CORESET.
  // - CORESET CCEs are divided by 2 to provide equal PDCCH resources to DL and UL.
  unsigned max_nof_candidates = (ss_info->coreset->get_nof_cces() / 2) / to_nof_cces(aggregation_level::n2);

  // > Subtract already scheduled PDCCHs.
  max_nof_candidates -= std::min((unsigned)cell_alloc[0].result.dl.dl_pdcchs.size(), max_nof_candidates);

  // > Ensure fairness in PDCCH allocation between DL and UL.
  // [Implementation-defined] To avoid running out of PDCCH candidates for UL allocation in multi-UE scenario and short
  // BW (e.g. TDD and 10Mhz BW), apply further limits on nof. UEs to be scheduled per slot.
  max_ue_grants_to_alloc = std::min(max_nof_candidates, max_ue_grants_to_alloc);

  const crb_interval& bwp_crb_limits = ss_info->dl_crb_lims;
  unsigned            max_nof_rbs    = std::min(bwp_crb_limits.length(), slice.remaining_rbs());

  return std::make_pair(max_ue_grants_to_alloc, std::min(max_nof_rbs / max_ue_grants_to_alloc, 4U));
}

unsigned intra_slice_scheduler::schedule_retx_candidates(du_cell_index_t               cell_index,
                                                         const dl_ran_slice_candidate& slice,
                                                         unsigned                      max_ue_grants_to_alloc)
{
  auto&                          slice_ues     = slice.get_slice_ues();
  const cell_resource_allocator& cell_alloc    = *cells[cell_index].cell_alloc;
  slot_point                     pdcch_slot    = cell_alloc.slot_tx();
  slot_point                     pdsch_slot    = slice.get_slot_tx();
  dl_harq_pending_retx_list      pending_harqs = cells[cell_index].cell_harqs->pending_dl_retxs();

  unsigned alloc_count = 0;
  for (auto it = pending_harqs.begin(); it != pending_harqs.end();) {
    // Note: During retx alloc, the pending HARQ list will mutate. So, we prefetch the next node.
    auto prev_it = it++;
    auto h       = *prev_it;

    if (h.get_grant_params().slice_id != slice.id()) {
      continue;
    }
    const slice_ue& u     = slice_ues[h.ue_index()];
    const ue_cell*  ue_cc = u.find_cell(cell_index);
    if (ue_cc == nullptr) {
      continue;
    }

    if (not can_allocate_pdsch(pdcch_slot, pdsch_slot, cell_index, u, *ue_cc)) {
      continue;
    }

    ue_pdsch_grant  grant{&u, cell_index, h.id()};
    dl_alloc_result result = ue_alloc.allocate_dl_grant(cell_index, slice, grant);

    if (result.status == alloc_status::skip_slot) {
      // Received signal to stop allocations in the slot.
      break;
    }

    if (result.status == alloc_status::success) {
      if (++alloc_count >= max_ue_grants_to_alloc) {
        // Maximum number of allocations reached.
        break;
      }
    }

    dl_attempts_count++;
    if (dl_attempts_count >= expert_cfg.max_pdcch_alloc_attempts_per_slot) {
      // Maximum number of attempts per slot reached.
      break;
    }
  }

  return alloc_count;
}

void intra_slice_scheduler::schedule_newtx_candidates(du_cell_index_t               cell_index,
                                                      const dl_ran_slice_candidate& slice,
                                                      unsigned                      max_ue_grants_to_alloc)
{
  const cell_resource_allocator& cell_alloc = *cells[cell_index].cell_alloc;
  auto [max_allocs, max_rbs_per_grant] =
      get_max_grants_and_dl_rb_grant_size(dl_newtx_candidates, cell_alloc, slice, max_ue_grants_to_alloc);
  if (max_rbs_per_grant == 0) {
    return;
  }
  max_ue_grants_to_alloc = max_allocs;

  unsigned alloc_count = 0;
  int      rbs_missing = 0;
  for (const auto& ue_candidate : dl_newtx_candidates) {
    // Determine the max grant size in RBs.
    unsigned max_grant_size = 0;
    if (alloc_count == max_ue_grants_to_alloc - 1) {
      // If we are in the last allocation of the slot, fill remaining RBs.
      max_grant_size = slice.remaining_rbs();
    } else {
      // Account the RBs that were left to be allocated earlier that changes on each allocation.
      max_grant_size = std::max((int)max_rbs_per_grant + rbs_missing, 0);
      max_grant_size = std::min(max_grant_size, slice.remaining_rbs());
    }
    if (max_grant_size == 0) {
      break;
    }

    // Allocate DL grant.
    dl_alloc_result result = ue_alloc.allocate_dl_grant(
        cell_index,
        slice,
        ue_pdsch_grant{ue_candidate.ue, cell_index, INVALID_HARQ_ID, ue_candidate.pending_bytes, max_rbs_per_grant});

    if (result.status == alloc_status::skip_slot) {
      // Received signal to stop allocations in the slot.
      break;
    }

    if (result.status == alloc_status::success) {
      if (++alloc_count >= max_ue_grants_to_alloc) {
        // Maximum number of allocations reached.
        break;
      }
      // Check if the grant was too small and we need to compensate in the next grants.
      rbs_missing += (max_rbs_per_grant - result.alloc_nof_rbs);
    }

    dl_attempts_count++;
    if (dl_attempts_count >= expert_cfg.max_pdcch_alloc_attempts_per_slot) {
      // Maximum number of attempts per slot reached.
      break;
    }
  }
}

bool intra_slice_scheduler::can_allocate_pdsch(slot_point      pdcch_slot,
                                               slot_point      pdsch_slot,
                                               du_cell_index_t cell_index,
                                               const slice_ue& u,
                                               const ue_cell&  ue_cc) const
{
  // Check if PDCCH is active for this slot (e.g. not in UL slot or measGap)
  if (not ue_cc.is_pdcch_enabled(pdcch_slot)) {
    return false;
  }

  // Check if PDSCH is active for the PDSCH slot.
  if (not ue_cc.is_pdsch_enabled(pdsch_slot)) {
    return false;
  }

  // Check if no PDSCH grant is already allocated for this UE in this slot.
  const auto& sched_pdschs = (*cells[cell_index].cell_alloc)[pdsch_slot].result.dl.ue_grants;
  if (std::any_of(sched_pdschs.begin(), sched_pdschs.end(), [&u](const auto& grant) {
        return grant.pdsch_cfg.rnti == u.crnti();
      })) {
    // UE already has a PDSCH grant in this slot. (e.g. a ReTx has took place earlier)
    return false;
  }

  return true;
}

std::optional<ue_pdsch_newtx_candidate> intra_slice_scheduler::create_newtx_dl_candidate(slot_point      pdcch_slot,
                                                                                         slot_point      pdsch_slot,
                                                                                         du_cell_index_t cell_index,
                                                                                         const slice_ue& u) const
{
  const ue_cell* ue_cc = u.find_cell(cell_index);
  if (ue_cc == nullptr or not ue_cc->is_active() or ue_cc->is_in_fallback_mode()) {
    return std::nullopt;
  }

  if (not can_allocate_pdsch(pdcch_slot, pdsch_slot, cell_index, u, *ue_cc)) {
    return std::nullopt;
  }

  if (not ue_cc->harqs.has_empty_dl_harqs()) {
    // No available HARQs or no pending data.
    if (not ue_cc->harqs.find_pending_dl_retx().has_value()) {
      // All HARQs are waiting for their respective HARQ-ACK. This may be a symptom of a long RTT for the PDSCH
      // and HARQ-ACK.
      logger.warning(
          "ue={} rnti={} PDSCH allocation skipped. Cause: All the HARQs are allocated and waiting for their "
          "respective HARQ-ACK. Check if any HARQ-ACK went missing in the lower layers or is arriving too late to "
          "the scheduler.",
          fmt::underlying(ue_cc->ue_index),
          ue_cc->rnti());
    }
    return std::nullopt;
  }

  // Check if the UE has pending data to transmit.
  unsigned pending_bytes = u.pending_dl_newtx_bytes();
  if (pending_bytes == 0) {
    return std::nullopt;
  }

  return ue_pdsch_newtx_candidate{&u, ue_cc, pending_bytes, forbid_sched_priority};
}

unsigned
intra_slice_scheduler::max_pdschs_to_alloc(slot_point pdcch_slot, slot_point pdsch_slot, du_cell_index_t cell_index)
{
  // We use signed integer to avoid unsigned overflow.
  int pdschs_to_alloc = MAX_DL_PDUS_PER_SLOT;

  // Determine how many PDCCHs can be allocated in this slot.
  auto& pdcch_res = (*cells[cell_index].cell_alloc)[pdcch_slot].result;
  pdschs_to_alloc = std::min({pdschs_to_alloc,
                              static_cast<int>(MAX_DL_PDCCH_PDUS_PER_SLOT - pdcch_res.dl.dl_pdcchs.size()),
                              static_cast<int>(expert_cfg.max_pdcch_alloc_attempts_per_slot - dl_attempts_count)});
  if (pdschs_to_alloc <= 0) {
    return 0;
  }

  // Determine how many UE DL PDUs can be allocated in this slot.
  auto& pdsch_res = (*cells[cell_index].cell_alloc)[pdsch_slot].result;
  pdschs_to_alloc = std::min(pdschs_to_alloc, static_cast<int>(MAX_UE_PDUS_PER_SLOT - pdsch_res.dl.ue_grants.size()));
  if (pdschs_to_alloc <= 0) {
    return 0;
  }

  // Determine how many PDSCHs can be allocated in this slot.
  const int max_pdschs = std::min(
      static_cast<int>(MAX_UE_PDUS_PER_SLOT + MAX_RAR_PDUS_PER_SLOT + MAX_PAGING_PDUS_PER_SLOT + MAX_SI_PDUS_PER_SLOT),
      static_cast<int>(expert_cfg.max_pdschs_per_slot));
  int allocated_pdschs = pdsch_res.dl.ue_grants.size() + pdsch_res.dl.bc.sibs.size() + pdsch_res.dl.rar_grants.size() +
                         pdsch_res.dl.paging_grants.size();
  pdschs_to_alloc = std::min(pdschs_to_alloc, max_pdschs - allocated_pdschs);

  return std::max(pdschs_to_alloc, 0);
}
