/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "pdcp_tx_metrics_test.h"
#include "lib/pdcp/pdcp_entity_impl.h"
#include "pdcp_test_vectors.h"
#include "srsran/pdcp/pdcp_config.h"
#include "srsran/support/test_utils.h"
#include <gtest/gtest.h>
#include <queue>

using namespace srsran;

TEST_F(pdcp_tx_metrics_container_test, init)
{
  pdcp_tx_metrics_container m = {};

  srslog::fetch_basic_logger("TEST", false).info("Metrics: {}", m);

  // Check values
  ASSERT_EQ(m.num_sdus, 0);
  ASSERT_EQ(m.num_sdu_bytes, 0);
  ASSERT_EQ(m.num_pdus, 0);
  ASSERT_EQ(m.num_pdu_bytes, 0);
  ASSERT_EQ(m.num_discard_timeouts, 0);
  ASSERT_EQ(m.sum_pdu_latency_ns, 0);
  ASSERT_EQ(m.counter, 0);
  for (auto freq : m.pdu_latency_hist) {
    ASSERT_EQ(freq, 0);
  }
  ASSERT_EQ(m.max_pdu_latency_ns, 0);
  ASSERT_EQ(m.cpu_usage, 0.0);

  {
    // Check fmt formatter
    fmt::memory_buffer buffer;
    fmt::format_to(std::back_inserter(buffer), "{}", m);
    std::string out_str = to_c_str(buffer);
    std::string exp_str = "num_sdus=0 num_sdu_bytes=0 num_pdus=0 num_pdu_bytes=0 num_discard_timeouts=0 "
                          "sum_pdu_latency=0ns sdu_latency_hist=[0 0 0 0 0 0 0 0] max_sdu_latency=0ns cpu_usage=0";
    srslog::fetch_basic_logger("TEST", false).info("out_str={}", out_str);
    srslog::fetch_basic_logger("TEST", false).info("exp_str={}", exp_str);
    ASSERT_EQ(out_str, exp_str);
  }

  {
    // Check custom formatter
    timer_duration dur{2}; // 2ms
    std::string    out_str = format_pdcp_tx_metrics(dur, m);
    std::string    exp_str =
        "num_sdus=0 sdu_rate= 0bps num_pdus=0 pdu_rate= 0bps num_discard_timeouts=0 "
        "sum_sdu_latency=0ns sdu_latency_hist=[ 0  0  0  0  0  0  0  0] max_pdu_latency=0us cpu_usage=0";
    srslog::fetch_basic_logger("TEST", false).info("out_str={}", out_str);
    srslog::fetch_basic_logger("TEST", false).info("exp_str={}", exp_str);
    ASSERT_EQ(out_str, exp_str);
  }
}

TEST_F(pdcp_tx_metrics_container_test, values)
{
  pdcp_tx_metrics_container m = {.num_sdus             = 4598,
                                 .num_sdu_bytes        = 39029,
                                 .num_pdus             = 9396,
                                 .num_pdu_bytes        = 69494,
                                 .num_discard_timeouts = 7,
                                 .sum_pdu_latency_ns   = 89684,
                                 .counter              = 4939,
                                 .cpu_usage            = 0.6,
                                 .pdu_latency_hist     = {999, 20, 400, 8000, 160000, 3200000, 64000000, 128},
                                 .max_pdu_latency_ns   = 54322};

  srslog::fetch_basic_logger("TEST", false).info("Metrics: {}", m);

  // Check values
  ASSERT_EQ(m.num_sdus, 4598);
  ASSERT_EQ(m.num_sdu_bytes, 39029);
  ASSERT_EQ(m.num_pdus, 9396);
  ASSERT_EQ(m.num_pdu_bytes, 69494);
  ASSERT_EQ(m.num_discard_timeouts, 7);
  ASSERT_EQ(m.sum_pdu_latency_ns, 89684);
  ASSERT_EQ(m.counter, 4939);
  std::array<uint32_t, 8> h = {999, 20, 400, 8000, 160000, 3200000, 64000000, 128};
  ASSERT_EQ(m.pdu_latency_hist, h);
  ASSERT_EQ(m.max_pdu_latency_ns, 54322);

  {
    // Check fmt formatter
    fmt::memory_buffer buffer;
    fmt::format_to(std::back_inserter(buffer), "{}", m);
    std::string out_str = to_c_str(buffer);
    std::string exp_str = "num_sdus=4598 num_sdu_bytes=39029 num_pdus=9396 num_pdu_bytes=69494 num_discard_timeouts=7 "
                          "sum_pdu_latency=89684ns sdu_latency_hist=[999 20 400 8000 160000 3200000 64000000 128] "
                          "max_sdu_latency=54322ns cpu_usage=0.6";
    srslog::fetch_basic_logger("TEST", false).info("out_str={}", out_str);
    srslog::fetch_basic_logger("TEST", false).info("exp_str={}", exp_str);
    ASSERT_EQ(out_str, exp_str);
  }

  {
    // Check custom formatter
    timer_duration dur{2}; // 2ms
    std::string    out_str = format_pdcp_tx_metrics(dur, m);
    std::string    exp_str =
        "num_sdus=4.6k sdu_rate=156Mbps num_pdus=9.4k pdu_rate=278Mbps num_discard_timeouts=7 sum_sdu_latency=89684ns "
        "sdu_latency_hist=[ 999  20  400 8.0k 160k 3.2M 64M  128] max_pdu_latency=54.322us cpu_usage=0.6";
    srslog::fetch_basic_logger("TEST", false).info("out_str={}", out_str);
    srslog::fetch_basic_logger("TEST", false).info("exp_str={}", exp_str);
    ASSERT_EQ(out_str, exp_str);
  }
}

/// \brief Test correct metrics counting of PDCP data SDUs/PDUs.
TEST_P(pdcp_tx_metrics_test, sdu_pdu)
{
  init(GetParam(), pdcp_rb_type::drb, pdcp_rlc_mode::am, pdcp_discard_timer::ms150);

  auto test_pdu_gen = [this](uint32_t tx_next) {
    srsran::test_delimit_logger delimiter("TX SDU/PDU metrics test. SN_SIZE={} COUNT={}", sn_size, tx_next);
    // Set state of PDCP entiy

    pdcp_tx_state st = {tx_next, tx_next};
    pdcp_tx->set_state(st);
    pdcp_tx->configure_security(sec_cfg, security::integrity_enabled::on, security::ciphering_enabled::on);

    // Write SDU
    byte_buffer sdu = byte_buffer::create(sdu1).value();
    pdcp_tx->handle_sdu(std::move(sdu));
    pdcp_tx->handle_transmit_notification(pdcp_compute_sn(st.tx_next + 1, sn_size));

    uint32_t exp_sdu_size = 2;
    tick_all(100);
    if (metrics_notif.metrics_list.empty()) {
      FAIL() << "No metrics notification received";
    }
    auto m = metrics_notif.metrics_list.back().tx;
    ASSERT_EQ(m.num_sdus, 1);
    ASSERT_EQ(m.num_sdu_bytes, exp_sdu_size);
    ASSERT_EQ(m.num_pdus, 1);
    ASSERT_EQ(m.num_pdu_bytes, pdu_hdr_len + exp_sdu_size + mac_hdr_len);
    ASSERT_EQ(m.num_discard_timeouts, 0);
    tick_all(100);
    if (metrics_notif.metrics_list.empty()) {
      FAIL() << "No metrics notification received";
    }
    m = metrics_notif.metrics_list.back().tx;
    ASSERT_EQ(m.num_discard_timeouts, 1);
  };

  if (config.sn_size == pdcp_sn_size::size12bits) {
    test_pdu_gen(0);
    test_pdu_gen(2048);
    test_pdu_gen(4095);
    test_pdu_gen(4096);
  } else if (config.sn_size == pdcp_sn_size::size18bits) {
    test_pdu_gen(0);
    test_pdu_gen(131072);
    test_pdu_gen(262144);
  } else {
    FAIL();
  }
}

///////////////////////////////////////////////////////////////////
// Finally, instantiate all testcases for each supported SN size //
///////////////////////////////////////////////////////////////////
std::string test_param_info_to_string(const ::testing::TestParamInfo<std::tuple<pdcp_sn_size, unsigned>>& info)
{
  fmt::memory_buffer buffer;
  fmt::format_to(std::back_inserter(buffer), "{}bit", pdcp_sn_size_to_uint(std::get<pdcp_sn_size>(info.param)));
  return fmt::to_string(buffer);
}

INSTANTIATE_TEST_SUITE_P(pdcp_tx_test_all_sn_sizes,
                         pdcp_tx_metrics_test,
                         ::testing::Combine(::testing::Values(pdcp_sn_size::size12bits, pdcp_sn_size::size18bits),
                                            ::testing::Values(1)),
                         test_param_info_to_string);

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
