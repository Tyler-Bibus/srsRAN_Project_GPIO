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

#include "../ue_context/ue.h"
#include "srsran/adt/slotted_array.h"

namespace srsran {

class slice_ue_repository;

/// UE managed by a given RAN slice for a given cell.
class slice_ue
{
public:
  explicit slice_ue(ue& u, ue_cell& ue_cc_, ran_slice_id_t slice_id);

  /// Returns DU UE index.
  du_ue_index_t ue_index() const { return u.ue_index; }

  /// Returns number of cells configured for the UE.
  unsigned nof_cells() const { return u.nof_cells(); }

  /// Returns UE C-RNTI.
  rnti_t crnti() const { return u.crnti; }

  /// Fetch UE carrier context managed by this slice for this UE.
  const ue_cell& get_cc() const { return ue_cc; }

  /// \brief Fetch UE cell based on DU cell index.
  const ue_cell* find_cell(du_cell_index_t cell_index) const { return u.find_cell(cell_index); }

  /// \brief Fetch UE cell based on UE-specific cell identifier. E.g. PCell corresponds to ue_cell_index==0.
  const ue_cell& get_cell(ue_cell_index_t ue_cell_index) const { return u.get_cell(ue_cell_index); }

  /// Determines if at least one SRB bearer of the UE is part of this slice.
  bool has_srb_bearers_in_slice() const
  {
    return contains(LCID_SRB0) or contains(LCID_SRB1) or contains(LCID_SRB2) or contains(LCID_SRB3);
  }

  /// Fetches the logical channel group associated with a given LCID.
  lcg_id_t get_lcg_id(lcid_t lcid) const
  {
    const auto& lchs = u.ue_cfg_dedicated()->logical_channels();
    return lchs.value().contains(lcid) ? lchs.value()[lcid]->lc_group : MAX_NOF_LCGS;
  }

  /// Determines if bearer with LCID is part of this slice.
  bool contains(lcid_t lcid) const { return u.dl_logical_channels().get_slice_id(lcid) == slice_id; }

  /// Determines if LCG-ID is part of this slice.
  bool contains(lcg_id_t lcg_id) const { return u.ul_logical_channels().get_slice_id(lcg_id) == slice_id; }

  /// Fetch DU cell index of UE's PCell.
  const ue_cell& get_pcell() const { return u.get_pcell(); }

  /// \brief Checks if there are DL pending bytes that are yet to be allocated in a DL HARQ.
  /// This method is faster than computing \c pending_dl_newtx_bytes() > 0.
  /// \remark Excludes SRB0 and UE Contention Resolution Identity CE.
  bool has_pending_dl_newtx_bytes() const { return u.dl_logical_channels().has_pending_bytes(slice_id); }

  /// \brief Computes the number of DL pending bytes for a given RAN slice that are not already allocated in a DL HARQ.
  /// \return Computed DL pending bytes.
  /// \remark Excludes SRB0 and UE Contention Resolution Identity CE.
  unsigned pending_dl_newtx_bytes() const { return u.dl_logical_channels().pending_bytes(slice_id); }

  /// \brief Computes the number of DL pending bytes for a given LCID that are not already allocated in a DL HARQ.
  /// \return Computed DL pending bytes.
  unsigned pending_dl_newtx_bytes(lcid_t lcid) const { return contains(lcid) ? u.pending_dl_newtx_bytes(lcid) : 0; }

  /// \brief Computes the number of UL pending bytes in bearers belonging to this slice that are not already allocated
  /// in a UL HARQ.
  unsigned pending_ul_newtx_bytes() const;

  /// \brief Computes the number of UL bytes that are yet to be received by the scheduler. This includes the UL bytes
  /// of grants not yet scheduled and the UL bytes of already scheduled grants whose CRC has not reached the scheduled.
  unsigned pending_ul_unacked_bytes(lcg_id_t lcg_id) const
  {
    return contains(lcg_id) ? u.ul_logical_channels().pending_bytes(lcg_id) : 0;
  }

  /// \brief Returns whether a SR indication handling is pending.
  bool has_pending_sr() const;

  /// Get QoS information of DRBs configured for the UE.
  logical_channel_config_list_ptr logical_channels() const { return u.ue_cfg_dedicated()->logical_channels(); };

  /// Average DL bit rate, in bps, for a given UE logical channel.
  double dl_avg_bit_rate(lcid_t lcid) const
  {
    return contains(lcid) ? u.dl_logical_channels().average_bit_rate(lcid) : 0;
  }

  /// Average UL bit rate, in bps, for a given UE logical channel group.
  double ul_avg_bit_rate(lcg_id_t lcg_id) const
  {
    return contains(lcg_id) ? u.ul_logical_channels().average_bit_rate(lcg_id) : 0;
  }

  /// Retrieve the Head-of-Line (HOL) Time-of-arrival (TOA) for a given logical channel.
  slot_point dl_hol_toa(lcid_t lcid) const
  {
    return contains(lcid) ? u.dl_logical_channels().hol_toa(lcid) : slot_point{};
  }

private:
  friend class slice_ue_repository;

  ue&                  u;
  ue_cell&             ue_cc;
  const ran_slice_id_t slice_id;
};

/// Container that store all UEs belonging to a RAN slice that are candidates for transmission.
class slice_ue_repository
{
public:
  slice_ue_repository(ran_slice_id_t slice_id_, du_cell_index_t cell_index_);

  bool empty() const { return ue_map.empty(); }

  size_t size() const { return ue_map.size(); }

  /// Determine if at least one bearer of the given UE is currently managed by this slice.
  bool contains(du_ue_index_t ue_index) const { return ue_map.contains(ue_index); }

  /// Determine if a (UE, LCID) tuple are managed by this slice.
  bool contains(du_ue_index_t ue_idx, lcid_t lcid) const { return contains(ue_idx) and ue_map[ue_idx].contains(lcid); }

  slice_ue&       operator[](du_ue_index_t ue_index) { return ue_map[ue_index]; }
  const slice_ue& operator[](du_ue_index_t ue_index) const { return ue_map[ue_index]; }

  /// Add a new UE to list of UEs (if not exists) and a new (UE, LCID) to the list of bearers managed by this slice.
  void add_logical_channel(ue& u, lcid_t lcid, lcg_id_t lcg_id);

  /// Remove a (UE, LCID) from the list of bearers managed by this slice.
  /// \remark UE is removed if all LCIDs of a UE are removed.
  void rem_logical_channel(du_ue_index_t ue_idx, lcid_t lcid);

  /// Remove UE from the list of managed UEs for this slice.
  void rem_ue(du_ue_index_t ue_index);

  auto begin() { return ue_map.begin(); }
  auto begin() const { return ue_map.begin(); }
  auto end() { return ue_map.end(); }
  auto end() const { return ue_map.end(); }

private:
  /// Add new UE to the RAN slice.
  bool add_ue(ue& u);

  const ran_slice_id_t  slice_id;
  const du_cell_index_t cell_index;

  slotted_id_table<du_ue_index_t, slice_ue, MAX_NOF_DU_UES> ue_map;
};

} // namespace srsran
