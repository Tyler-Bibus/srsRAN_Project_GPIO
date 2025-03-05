/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "../test_utils/dummy_test_components.h"
#include "lib/scheduler/config/sched_config_manager.h"
#include "lib/scheduler/logging/scheduler_metrics_handler.h"
#include "lib/scheduler/logging/scheduler_result_logger.h"
#include "lib/scheduler/pdcch_scheduling/pdcch_resource_allocator_impl.h"
#include "lib/scheduler/pucch_scheduling/pucch_allocator_impl.h"
#include "lib/scheduler/uci_scheduling/uci_allocator_impl.h"
#include "lib/scheduler/ue_context/ue.h"
#include "lib/scheduler/ue_scheduling/ue_cell_grid_allocator.h"
#include "tests/test_doubles/scheduler/scheduler_config_helper.h"
#include "srsran/ran/du_types.h"
#include "srsran/ran/duplex_mode.h"
#include "srsran/ran/pdcch/search_space.h"
#include "srsran/scheduler/config/logical_channel_config_factory.h"
#include "srsran/scheduler/config/scheduler_expert_config_factory.h"
#include <gtest/gtest.h>
#include <tests/test_doubles/scheduler/scheduler_result_test.h>

using namespace srsran;

class ue_grid_allocator_tester : public ::testing::TestWithParam<duplex_mode>
{
protected:
  ue_grid_allocator_tester(
      scheduler_expert_config sched_cfg_ = config_helpers::make_default_scheduler_expert_config()) :
    sched_cfg(sched_cfg_),
    cell_cfg(*[this]() {
      cfg_builder_params.dl_f_ref_arfcn = GetParam() == duplex_mode::FDD ? 530000 : 520002;
      cfg_builder_params.scs_common =
          GetParam() == duplex_mode::FDD ? subcarrier_spacing::kHz15 : subcarrier_spacing::kHz30;
      cfg_builder_params.band           = band_helper::get_band_from_dl_arfcn(cfg_builder_params.dl_f_ref_arfcn);
      cfg_builder_params.channel_bw_mhz = bs_channel_bandwidth::MHz20;
      auto* cfg =
          cfg_mng.add_cell(sched_config_helper::make_default_sched_cell_configuration_request(cfg_builder_params));
      srsran_assert(cfg != nullptr, "Cell configuration failed");
      return cfg;
    }()),
    slice_ues(ran_slice_id_t{0}, to_du_cell_index(0)),
    current_slot(cfg_builder_params.scs_common, 0)
  {
    logger.set_level(srslog::basic_levels::debug);
    srslog::init();

    // Initialize resource grid.
    slot_indication();

    alloc.add_cell(to_du_cell_index(0), pdcch_alloc, uci_alloc, res_grid);
  }

  slot_point get_next_ul_slot(const slot_point starting_slot) const
  {
    slot_point next_slot = starting_slot + cfg_builder_params.min_k2;
    while (not cell_cfg.is_fully_ul_enabled(next_slot)) {
      ++next_slot;
    }
    return next_slot;
  }

  void slot_indication(std::function<void()> on_each_slot = []() {})
  {
    ++current_slot;
    logger.set_context(current_slot.sfn(), current_slot.slot_index());

    res_grid.slot_indication(current_slot);
    cell_harqs.slot_indication(current_slot);
    pdcch_alloc.slot_indication(current_slot);
    pucch_alloc.slot_indication(current_slot);
    uci_alloc.slot_indication(current_slot);
    ues.slot_indication(current_slot);

    on_each_slot();

    alloc.post_process_results();

    // Log scheduler results.
    res_logger.on_scheduler_result(res_grid[0].result);
  }

  bool run_until(std::function<void()> to_run, unique_function<bool()> until, unsigned max_slot_count = 1000)
  {
    if (until()) {
      return true;
    }
    for (unsigned count = 0; count != max_slot_count; ++count) {
      slot_indication(to_run);
      if (until()) {
        return true;
      }
    }
    return false;
  }

  ue& add_ue(du_ue_index_t ue_index, const std::initializer_list<lcid_t>& lcids_to_activate)
  {
    sched_ue_creation_request_message ue_creation_req =
        sched_config_helper::create_default_sched_ue_creation_request(cfg_builder_params);
    ue_creation_req.ue_index = ue_index;
    ue_creation_req.crnti    = to_rnti(0x4601 + (unsigned)ue_index);
    for (lcid_t lcid : lcids_to_activate) {
      ue_creation_req.cfg.lc_config_list->push_back(config_helpers::create_default_logical_channel_config(lcid));
    }

    return add_ue(ue_creation_req);
  }

  ue& add_ue(const sched_ue_creation_request_message& ue_creation_req)
  {
    auto ev = cfg_mng.add_ue(ue_creation_req);
    ues.add_ue(
        std::make_unique<ue>(ue_creation_command{ev.next_config(), ue_creation_req.starts_in_fallback, cell_harqs}));
    for (const auto& lc_cfg : *ue_creation_req.cfg.lc_config_list) {
      slice_ues.add_logical_channel(ues[ue_creation_req.ue_index], lc_cfg.lcid, lc_cfg.lc_group);
    }
    ev.notify_completion();
    return ues[ue_creation_req.ue_index];
  }

  void push_dl_bs(du_ue_index_t ue_index, lcid_t lcid, unsigned bytes)
  {
    ues[ue_index].handle_dl_buffer_state_indication(lcid, bytes);
  }

  void allocate_dl_newtx_grant(const ue_dl_newtx_grant_request& grant)
  {
    dl_ran_slice_candidate slice{slice_inst, current_slot, rrm_policy.max_prb};
    alloc.allocate_newtx_dl_grant(to_du_cell_index(0), slice, grant);
  }

  void allocate_dl_retx_grant(const ue_dl_retx_grant_request& grant)
  {
    dl_ran_slice_candidate slice{slice_inst, current_slot, rrm_policy.max_prb};
    alloc.allocate_retx_dl_grant(to_du_cell_index(0), slice, grant);
  }

  ul_alloc_result allocate_ul_newtx_grant(const ue_ul_newtx_grant_request& grant)
  {
    return allocate_ul_newtx_grant(grant, get_next_ul_slot(current_slot));
  }

  ul_alloc_result allocate_ul_newtx_grant(const ue_ul_newtx_grant_request& grant, slot_point pusch_slot)
  {
    if (not cell_cfg.is_dl_enabled(current_slot)) {
      return ul_alloc_result{alloc_status::invalid_params};
    }
    ul_ran_slice_candidate slice{slice_inst, pusch_slot, rrm_policy.max_prb};
    return alloc.allocate_newtx_ul_grant(to_du_cell_index(0), slice, grant);
  }

  scheduler_expert_config                 sched_cfg;
  scheduler_ue_expert_config              expert_cfg{sched_cfg.ue};
  sched_cfg_dummy_notifier                mac_notif;
  scheduler_ue_metrics_dummy_notifier     metrics_notif;
  scheduler_ue_metrics_dummy_configurator metrics_ue_handler;
  scheduler_metrics_handler               metrics{std::chrono::milliseconds{0}, metrics_notif};

  cell_config_builder_params cfg_builder_params;
  sched_config_manager       cfg_mng{scheduler_config{sched_cfg, mac_notif, metrics_notif}, metrics};
  const cell_configuration&  cell_cfg;

  cell_harq_manager       cell_harqs{MAX_NOF_DU_UES,
                               MAX_NOF_HARQS,
                               std::make_unique<scheduler_harq_timeout_dummy_notifier>()};
  cell_resource_allocator res_grid{cell_cfg};

  pdcch_resource_allocator_impl pdcch_alloc{cell_cfg};
  pucch_allocator_impl pucch_alloc{cell_cfg, expert_cfg.max_pucchs_per_slot, expert_cfg.max_ul_grants_per_slot};
  uci_allocator_impl   uci_alloc{pucch_alloc};

  srslog::basic_logger&   logger{srslog::fetch_basic_logger("SCHED")};
  scheduler_result_logger res_logger{false, cell_cfg.pci};

  ue_repository           ues;
  slice_ue_repository     slice_ues;
  slice_rrm_policy_config rrm_policy;
  ran_slice_instance      slice_inst{ran_slice_id_t{0}, cell_cfg, rrm_policy};
  ue_cell_grid_allocator  alloc{expert_cfg, ues, logger};

  slot_point current_slot;
};

TEST_P(ue_grid_allocator_tester,
       when_ue_dedicated_ss_is_css_then_allocation_is_within_coreset_start_crb_and_coreset0_end_crb)
{
  static const unsigned nof_bytes_to_schedule = 40U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  // Change SS type to common.
  (*ue_creation_req.cfg.cells)[0]
      .serv_cell_cfg.init_dl_bwp.pdcch_cfg->search_spaces[0]
      .set_non_ss0_monitored_dci_formats(search_space_configuration::common_dci_format{.f0_0_and_f1_0 = true});
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);

  ue& u = add_ue(ue_creation_req);

  const crb_interval crbs =
      get_coreset_crbs((*ue_creation_req.cfg.cells)[0].serv_cell_cfg.init_dl_bwp.pdcch_cfg.value().coresets.back());
  const crb_interval crb_lims = {
      crbs.start(), crbs.start() + cell_cfg.dl_cfg_common.init_dl_bwp.pdcch_common.coreset0->coreset0_crbs().length()};

  ue_dl_newtx_grant_request grant_req{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant_req); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  ASSERT_TRUE(crb_lims.contains(res_grid[0].result.dl.ue_grants.back().pdsch_cfg.rbs.type1()));
}

TEST_P(ue_grid_allocator_tester, when_using_non_fallback_dci_format_use_mcs_table_set_in_pdsch_cfg)
{
  static const unsigned nof_bytes_to_schedule = 40U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  // Change PDSCH MCS table to be used when using non-fallback DCI format.
  (*ue_creation_req.cfg.cells)[0].serv_cell_cfg.init_dl_bwp.pdsch_cfg->mcs_table = srsran::pdsch_mcs_table::qam256;
  ue_creation_req.ue_index                                                       = to_du_ue_index(0);
  ue_creation_req.crnti                                                          = to_rnti(0x4601);

  const ue& u = add_ue(ue_creation_req);

  // SearchSpace#2 uses non-fallback DCI format hence the MCS table set in dedicated PDSCH configuration must be used.
  const ue_dl_newtx_grant_request grant{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  ASSERT_EQ(res_grid[0].result.dl.ue_grants.back().pdsch_cfg.codewords.back().mcs_table,
            srsran::pdsch_mcs_table::qam256);
}

TEST_P(ue_grid_allocator_tester, allocates_pdsch_restricted_to_recommended_max_nof_rbs)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  static const unsigned sched_bytes             = 2000U;
  const unsigned        max_nof_rbs_to_schedule = 10U;

  const ue_dl_newtx_grant_request grant1{slice_ues[u1.ue_index], sched_bytes, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  // Successfully allocates PDSCH corresponding to the grant.
  ASSERT_GE(find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.rbs.type1().length(),
            grant1.max_nof_rbs);
}

TEST_P(ue_grid_allocator_tester, allocates_pusch_restricted_to_recommended_max_nof_rbs)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  const unsigned recommended_nof_bytes_to_schedule = 2000U;
  const unsigned max_nof_rbs_to_schedule           = 10U;

  const ue_ul_newtx_grant_request grant1{
      slice_ues[u1.ue_index], recommended_nof_bytes_to_schedule, max_nof_rbs_to_schedule};
  ASSERT_TRUE(run_until([&]() { allocate_ul_newtx_grant(grant1); },
                        [&]() { return find_ue_pusch(u1.crnti, res_grid[0].result.ul) != nullptr; }));
  // Successfully allocates PUSCH corresponding to the grant.
  ASSERT_EQ(find_ue_pusch(u1.crnti, res_grid[0].result.ul)->pusch_cfg.rbs.type1().length(), grant1.max_nof_rbs);
}

TEST_P(ue_grid_allocator_tester, does_not_allocate_pusch_with_all_remaining_rbs_if_its_a_sr_indication)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  ue& u1                   = add_ue(ue_creation_req);
  // Trigger a SR indication.
  u1.handle_sr_indication();

  const ue_ul_newtx_grant_request grant1{slice_ues[u1.ue_index], u1.pending_ul_newtx_bytes()};

  const crb_interval cell_crbs = {cell_cfg.ul_cfg_common.init_ul_bwp.generic_params.crbs.start(),
                                  cell_cfg.ul_cfg_common.init_ul_bwp.generic_params.crbs.stop()};

  ASSERT_TRUE(run_until([&]() { allocate_ul_newtx_grant(grant1); },
                        [&]() { return find_ue_pusch(u1.crnti, res_grid[0].result.ul) != nullptr; }));
  // Successfully allocates PUSCH corresponding to the grant.
  ASSERT_LT(find_ue_pusch(u1.crnti, res_grid[0].result.ul)->pusch_cfg.rbs.type1().length(), cell_crbs.length());
}

TEST_P(ue_grid_allocator_tester, no_two_pdschs_are_allocated_in_same_slot_for_a_ue)
{
  static const unsigned nof_bytes_to_schedule = 400U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // First PDSCH grant for the UE.
  const ue_dl_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  // Second PDSCH grant for the UE.
  const ue_dl_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until(
      [&]() {
        allocate_dl_newtx_grant(grant1);
        allocate_dl_newtx_grant(grant2);
      },
      [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));

  // Only one PDSCH per slot per UE.
  ASSERT_EQ(res_grid[0].result.dl.ue_grants.size(), 1);
}

TEST_P(ue_grid_allocator_tester, no_two_puschs_are_allocated_in_same_slot_for_a_ue)
{
  static const unsigned nof_bytes_to_schedule = 400U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // First PUSCH grant for the UE.
  const ue_ul_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  // Second PUSCH grant for the UE.
  const ue_ul_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until(
      [&]() {
        allocate_ul_newtx_grant(grant1);
        allocate_ul_newtx_grant(grant2);
      },
      [&]() { return find_ue_pusch(u.crnti, res_grid[0].result.ul) != nullptr; }));

  // Only one PUSCH per slot per UE.
  ASSERT_EQ(res_grid[0].result.ul.puschs.size(), 1);
}

TEST_P(ue_grid_allocator_tester, consecutive_puschs_for_a_ue_are_allocated_in_increasing_order_of_time)
{
  static const unsigned nof_bytes_to_schedule = 400U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // First PUSCH grant for the UE.
  const ue_ul_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  slot_point pusch_slot;
  ASSERT_TRUE(run_until(
      [&]() {
        pusch_slot = get_next_ul_slot(current_slot);
        allocate_ul_newtx_grant(grant1, pusch_slot);
      },
      [&]() { return find_ue_pusch(u.crnti, res_grid[0].result.ul) != nullptr; }));

  // Second PUSCH grant for the UE trying to allocate PUSCH in a slot previous to grant1.
  const ue_ul_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ul_alloc_result result = {alloc_status::invalid_params};
  ASSERT_FALSE(run_until([&]() { result = allocate_ul_newtx_grant(grant2, pusch_slot - 1); },
                         [&]() { return result.status == alloc_status::success; },
                         1));
}

TEST_P(ue_grid_allocator_tester, consecutive_pdschs_for_a_ue_are_allocated_in_increasing_order_of_time)
{
  static const unsigned nof_bytes_to_schedule = 400U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // First PDSCH grant for the UE.
  const ue_dl_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  slot_point last_pdsch_slot = current_slot;

  // Second PDSCH grant in the same slot for the UE.
  const ue_dl_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant2); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  ASSERT_GE(current_slot, last_pdsch_slot);
}

TEST_P(ue_grid_allocator_tester,
       ack_slot_of_consecutive_pdschs_for_a_ue_must_be_greater_than_or_equal_to_last_ack_slot_allocated)
{
  static const unsigned nof_bytes_to_schedule = 400U;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // First PDSCH grant for the UE.
  const ue_dl_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  slot_point last_pdsch_ack_slot = current_slot + find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants)->context.k1;

  // Second PDSCH grant in the same slot for the UE.
  const ue_dl_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant2); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  ASSERT_GE(current_slot + find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants)->context.k1, last_pdsch_ack_slot);
}

TEST_P(ue_grid_allocator_tester, successfully_allocated_pdsch_even_with_large_gap_to_last_pdsch_slot_allocated)
{
  static const unsigned nof_bytes_to_schedule                       = 8U;
  const unsigned        nof_slot_until_pdsch_is_allocated_threshold = SCHEDULER_MAX_K0;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // Ensure current slot is the middle of 1024 SFNs. i.e. current slot=511.0
  while (current_slot.sfn() != NOF_SFNS / 2) {
    slot_indication();
  }

  // First PDSCH grant for the UE.
  const ue_dl_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));

  // Ensure next PDSCH to be allocated slot is after wrap around of 1024 SFNs (large gap to last allocated PDSCH slot)
  // and current slot value is less than last allocated PDSCH slot. e.g. next PDSCH to be allocated slot=SFN 2, slot 2
  // after wrap around of 1024 SFNs.
  for (unsigned i = 0; i < current_slot.nof_slots_per_system_frame() / 2 + current_slot.nof_slots_per_frame(); ++i) {
    slot_indication();
  }

  // Next PDSCH grant to be allocated.
  const ue_dl_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant2); },
                        [&]() { return find_ue_pdsch(u.crnti, res_grid[0].result.dl.ue_grants) != nullptr; },
                        nof_slot_until_pdsch_is_allocated_threshold));
}

TEST_P(ue_grid_allocator_tester, successfully_allocates_pdsch_with_gbr_lc_priortized_over_non_gbr_lc)
{
  const lcg_id_t lcg_id              = uint_to_lcg_id(2);
  const lcid_t   gbr_bearer_lcid     = uint_to_lcid(6);
  const lcid_t   non_gbr_bearer_lcid = uint_to_lcid(5);

  // Add UE.
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  ue& u1                   = add_ue(ue_creation_req);

  // Reconfigure UE to include non-GBR bearer and GBR bearer.
  sched_ue_reconfiguration_message reconf_msg{
      .ue_index = ue_creation_req.ue_index, .crnti = ue_creation_req.crnti, .cfg = ue_creation_req.cfg};
  sched_ue_config_request& cfg_req = reconf_msg.cfg;
  cfg_req.lc_config_list.emplace();
  cfg_req.lc_config_list->resize(4);
  (*cfg_req.lc_config_list)[0]          = config_helpers::create_default_logical_channel_config(lcid_t::LCID_SRB0);
  (*cfg_req.lc_config_list)[1]          = config_helpers::create_default_logical_channel_config(lcid_t::LCID_SRB1);
  (*cfg_req.lc_config_list)[2]          = config_helpers::create_default_logical_channel_config(non_gbr_bearer_lcid);
  (*cfg_req.lc_config_list)[2].lc_group = lcg_id;
  (*cfg_req.lc_config_list)[2].qos.emplace();
  (*cfg_req.lc_config_list)[2].qos->qos = *get_5qi_to_qos_characteristics_mapping(uint_to_five_qi(9));
  (*cfg_req.lc_config_list)[3]          = config_helpers::create_default_logical_channel_config(gbr_bearer_lcid);
  // Put GBR bearer in a different LCG than non-GBR bearer.
  (*cfg_req.lc_config_list)[3].lc_group = uint_to_lcg_id(lcg_id - 1);
  (*cfg_req.lc_config_list)[3].qos.emplace();
  (*cfg_req.lc_config_list)[3].qos->qos          = *get_5qi_to_qos_characteristics_mapping(uint_to_five_qi(1));
  (*cfg_req.lc_config_list)[3].qos->gbr_qos_info = gbr_qos_flow_information{128000, 128000, 128000, 128000};
  ue_config_update_event ev                      = cfg_mng.update_ue(reconf_msg);
  u1.handle_reconfiguration_request({ev.next_config()});
  u1.handle_config_applied();

  // Add LCID to the bearers of the UE belonging to this slice.
  for (const auto& lc_cfg : *cfg_req.lc_config_list) {
    slice_ues.add_logical_channel(u1, lc_cfg.lcid, lc_cfg.lc_group);
  }

  // Push buffer state update to both bearers.
  push_dl_bs(u1.ue_index, gbr_bearer_lcid, 200);
  push_dl_bs(u1.ue_index, non_gbr_bearer_lcid, 1500);

  static const unsigned sched_bytes = 2000U;

  const ue_dl_newtx_grant_request grant1{slice_ues[u1.ue_index], sched_bytes};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));

  const auto* ue_pdsch = find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants);
  ASSERT_TRUE(not ue_pdsch->tb_list.empty());
  ASSERT_TRUE(not ue_pdsch->tb_list.back().lc_chs_to_sched.empty());
  // TB info contains GBR LC channel first and then non-GBR LC channel.
  ASSERT_EQ(ue_pdsch->tb_list.back().lc_chs_to_sched.front().lcid, gbr_bearer_lcid);
}

TEST_P(ue_grid_allocator_tester, successfully_allocated_pusch_even_with_large_gap_to_last_pusch_slot_allocated)
{
  static const unsigned nof_bytes_to_schedule                       = 400U;
  const unsigned        nof_slot_until_pusch_is_allocated_threshold = SCHEDULER_MAX_K2;

  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);

  const ue& u = add_ue(ue_creation_req);

  // Ensure current slot is the middle of 1024 SFNs. i.e. current slot=511.0
  while (current_slot.sfn() != NOF_SFNS / 2) {
    slot_indication();
  }

  // First PUSCH grant for the UE.
  const ue_ul_newtx_grant_request grant1{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_ul_newtx_grant(grant1); },
                        [&]() { return find_ue_pusch(u.crnti, res_grid[0].result.ul.puschs) != nullptr; }));

  // Ensure next PUSCH to be allocated slot is after wrap around of 1024 SFNs (large gap to last allocated PUSCH slot)
  // and current slot value is less than last allocated PUSCH slot. e.g. next PUSCH to be allocated slot=SFN 2, slot 2
  // after wrap around of 1024 SFNs.
  for (unsigned i = 0; i < current_slot.nof_slots_per_system_frame() / 2 + current_slot.nof_slots_per_frame(); ++i) {
    slot_indication();
  }

  // Second PUSCH grant for the UE.
  const ue_ul_newtx_grant_request grant2{slice_ues[u.ue_index], nof_bytes_to_schedule};

  ASSERT_TRUE(run_until([&]() { return allocate_ul_newtx_grant(grant2).status == alloc_status::success; },
                        [&]() { return find_ue_pusch(u.crnti, res_grid[0].result.ul.puschs) != nullptr; },
                        nof_slot_until_pusch_is_allocated_threshold));
}

class ue_grid_allocator_remaining_rbs_alloc_tester : public ue_grid_allocator_tester
{
public:
  ue_grid_allocator_remaining_rbs_alloc_tester() :
    ue_grid_allocator_tester(([]() {
      scheduler_expert_config sched_cfg_   = config_helpers::make_default_scheduler_expert_config();
      sched_cfg_.ue.max_ul_grants_per_slot = 2;
      sched_cfg_.ue.max_pucchs_per_slot    = 2;
      return sched_cfg_;
    }()))
  {
  }
};

TEST_P(ue_grid_allocator_remaining_rbs_alloc_tester, remaining_dl_rbs_are_allocated_if_max_pucch_per_slot_is_reached)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);
  ue_creation_req.ue_index = to_du_ue_index(1);
  ue_creation_req.crnti    = to_rnti(0x4602);
  const ue& u2             = add_ue(ue_creation_req);

  static const unsigned           sched_bytes = 20U;
  const ue_dl_newtx_grant_request grant1{slice_ues[u1.ue_index], sched_bytes};

  // Since UE dedicated SearchSpace is a UE specific SearchSpace (Not CSS). Entire BWP CRBs can be used for
  // allocation.
  const unsigned                  total_crbs = cell_cfg.dl_cfg_common.init_dl_bwp.generic_params.crbs.length();
  const ue_dl_newtx_grant_request grant2{slice_ues[u2.ue_index], sched_bytes};

  ASSERT_TRUE(run_until(
      [&]() {
        allocate_dl_newtx_grant(grant1);
        allocate_dl_newtx_grant(grant2);
      },
      [&]() {
        return find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants) != nullptr and
               find_ue_pdsch(u2.crnti, res_grid[0].result.dl.ue_grants) != nullptr;
      }));
  // Successfully allocates PDSCH corresponding to the grant.
  ASSERT_GE(find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.codewords.back().tb_size_bytes,
            sched_bytes);

  // Since UE dedicated SearchSpace is a UE specific SearchSpace (Not CSS). Entire BWP CRBs can be used for
  // allocation.
  const unsigned crbs_allocated =
      find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.rbs.type1().length();

  // Allocates all remaining RBs to UE2.
  ASSERT_EQ(find_ue_pdsch(u2.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.rbs.type1().length(),
            (total_crbs - crbs_allocated));
}

class ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester : public ue_grid_allocator_tester
{
public:
  ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester() :
    ue_grid_allocator_tester(([]() {
      scheduler_expert_config sched_cfg_ = config_helpers::make_default_scheduler_expert_config();
      sched_cfg_.ue.pdsch_nof_rbs        = {20, 40};
      sched_cfg_.ue.pusch_nof_rbs        = {20, 40};
      return sched_cfg_;
    }()))
  {
  }
};

TEST_P(ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester,
       allocates_pdsch_with_expert_cfg_min_nof_rbs_even_if_rbs_required_to_schedule_is_low)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  // Ensure the buffer status is low enough such that < 20 RBs (configured in constructor) are required to schedule.
  static const unsigned sched_bytes             = 20U;
  const unsigned        max_nof_rbs_to_schedule = 10U;

  const ue_dl_newtx_grant_request grant1{slice_ues[u1.ue_index], sched_bytes, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  // Successfully allocates PDSCH.
  ASSERT_EQ(find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.rbs.type1().length(),
            std::max(expert_cfg.pdsch_nof_rbs.start(), max_nof_rbs_to_schedule));
}

TEST_P(ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester,
       allocates_pdsch_with_expert_cfg_max_nof_rbs_even_if_rbs_required_to_schedule_is_high)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  // Ensure the buffer status is high enough such that > 40 RBs (configured in constructor) are required to schedule.
  static const unsigned sched_bytes             = 20000U;
  const unsigned        max_nof_rbs_to_schedule = 273U;

  const ue_dl_newtx_grant_request grant1{slice_ues[u1.ue_index], sched_bytes, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  // Successfully allocates PDSCH.
  ASSERT_EQ(find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.rbs.type1().length(),
            std::min(expert_cfg.pdsch_nof_rbs.stop(), max_nof_rbs_to_schedule));
}

TEST_P(ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester,
       allocates_pusch_with_expert_cfg_min_nof_rbs_even_if_rbs_required_to_schedule_is_low)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  // Ensure the buffer status is low enough such that < 20 RBs (configured in constructor) are required to schedule.
  const unsigned recommended_nof_bytes_to_schedule = 20U;
  const unsigned max_nof_rbs_to_schedule           = 10U;

  const ue_ul_newtx_grant_request grant1{
      slice_ues[u1.ue_index], recommended_nof_bytes_to_schedule, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_ul_newtx_grant(grant1); },
                        [&]() { return find_ue_pusch(u1.crnti, res_grid[0].result.ul) != nullptr; }));
  // Successfully allocates PUSCH.
  ASSERT_EQ(find_ue_pusch(u1.crnti, res_grid[0].result.ul)->pusch_cfg.rbs.type1().length(),
            std::max(expert_cfg.pdsch_nof_rbs.start(), max_nof_rbs_to_schedule));
}

TEST_P(ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester,
       allocates_pusch_with_expert_cfg_max_nof_rbs_even_if_rbs_required_to_schedule_is_high)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  // Ensure the buffer status is high enough such that > 40 RBs (configured in constructor) are required to schedule.
  const unsigned recommended_nof_bytes_to_schedule = 200000U;
  const unsigned max_nof_rbs_to_schedule           = 273U;

  const ue_ul_newtx_grant_request grant1{
      slice_ues[u1.ue_index], recommended_nof_bytes_to_schedule, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_ul_newtx_grant(grant1); },
                        [&]() { return find_ue_pusch(u1.crnti, res_grid[0].result.ul) != nullptr; }));
  // Successfully allocates PUSCH.
  ASSERT_EQ(find_ue_pusch(u1.crnti, res_grid[0].result.ul)->pusch_cfg.rbs.type1().length(),
            std::min(expert_cfg.pdsch_nof_rbs.stop(), max_nof_rbs_to_schedule));
}

class ue_grid_allocator_expert_cfg_pxsch_crb_limits_tester : public ue_grid_allocator_tester
{
public:
  ue_grid_allocator_expert_cfg_pxsch_crb_limits_tester() :
    ue_grid_allocator_tester(([]() {
      scheduler_expert_config sched_cfg_ = config_helpers::make_default_scheduler_expert_config();
      sched_cfg_.ue.pdsch_crb_limits     = {20, 40};
      sched_cfg_.ue.pusch_crb_limits     = {20, 40};
      return sched_cfg_;
    }()))
  {
    // Assume SS#2 is USS configured with DCI format 1_1/0_1 and is the only SS used for UE PDSCH/PUSCH scheduling.
    const prb_interval pdsch_prbs =
        crb_to_prb(cell_cfg.dl_cfg_common.init_dl_bwp.generic_params.crbs, sched_cfg.ue.pdsch_crb_limits);
    pdsch_vrb_limits = vrb_interval{pdsch_prbs.start(), pdsch_prbs.stop()};
    pusch_vrb_limits = rb_helper::crb_to_vrb_ul_non_interleaved(
        sched_cfg.ue.pusch_crb_limits, cell_cfg.ul_cfg_common.init_ul_bwp.generic_params.crbs.start());
  }

protected:
  vrb_interval pdsch_vrb_limits;
  vrb_interval pusch_vrb_limits;
};

TEST_P(ue_grid_allocator_expert_cfg_pxsch_crb_limits_tester, allocates_pdsch_within_expert_cfg_pdsch_rb_limits)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  // Ensure the buffer status is high enough such that > 20 RBs (configured in constructor) are required to schedule.
  static const unsigned sched_bytes             = 20000U;
  const unsigned        max_nof_rbs_to_schedule = 273U;

  const ue_dl_newtx_grant_request grant1{slice_ues[u1.ue_index], sched_bytes, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_dl_newtx_grant(grant1); },
                        [&]() { return find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants) != nullptr; }));
  // Successfully allocates PDSCH within RB limits.
  ASSERT_EQ(find_ue_pdsch(u1.crnti, res_grid[0].result.dl.ue_grants)->pdsch_cfg.rbs.type1(), pdsch_vrb_limits);
}

TEST_P(ue_grid_allocator_expert_cfg_pxsch_crb_limits_tester, allocates_pusch_within_expert_cfg_pusch_rb_limits)
{
  sched_ue_creation_request_message ue_creation_req =
      sched_config_helper::create_default_sched_ue_creation_request(this->cfg_builder_params);
  ue_creation_req.ue_index = to_du_ue_index(0);
  ue_creation_req.crnti    = to_rnti(0x4601);
  const ue& u1             = add_ue(ue_creation_req);

  // Ensure the buffer status is high enough such that > 20 RBs (configured in constructor) are required to schedule.
  const unsigned recommended_nof_bytes_to_schedule = 200000U;
  const unsigned max_nof_rbs_to_schedule           = 273U;

  const ue_ul_newtx_grant_request grant1{
      slice_ues[u1.ue_index], recommended_nof_bytes_to_schedule, max_nof_rbs_to_schedule};

  ASSERT_TRUE(run_until([&]() { allocate_ul_newtx_grant(grant1); },
                        [&]() { return find_ue_pusch(u1.crnti, res_grid[0].result.ul) != nullptr; }));
  // Successfully allocates PUSCH within RB limits.
  ASSERT_EQ(find_ue_pusch(u1.crnti, res_grid[0].result.ul)->pusch_cfg.rbs.type1(), pusch_vrb_limits);
}

INSTANTIATE_TEST_SUITE_P(ue_grid_allocator_test,
                         ue_grid_allocator_tester,
                         testing::Values(duplex_mode::FDD, duplex_mode::TDD));

INSTANTIATE_TEST_SUITE_P(ue_grid_allocator_test,
                         ue_grid_allocator_remaining_rbs_alloc_tester,
                         testing::Values(duplex_mode::FDD, duplex_mode::TDD));

INSTANTIATE_TEST_SUITE_P(ue_grid_allocator_test,
                         ue_grid_allocator_expert_cfg_pxsch_nof_rbs_limits_tester,
                         testing::Values(duplex_mode::FDD, duplex_mode::TDD));

INSTANTIATE_TEST_SUITE_P(ue_grid_allocator_test,
                         ue_grid_allocator_expert_cfg_pxsch_crb_limits_tester,
                         testing::Values(duplex_mode::FDD, duplex_mode::TDD));
