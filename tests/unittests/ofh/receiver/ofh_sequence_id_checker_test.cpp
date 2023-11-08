/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "../../../../lib/ofh/receiver/ofh_sequence_id_checker.h"
#include <gtest/gtest.h>

using namespace srsran;
using namespace ofh;

TEST(ofh_sequence_id_checker, first_message_is_always_ok)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 1;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, consecutive_messages_is_ok)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 1;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  ++seq_id;
  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  ++seq_id;
  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, message_from_the_past_is_detected)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 1;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  --seq_id;

  ASSERT_EQ(-2, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, message_from_the_past_is_detected_with_big_difference)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 14;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  seq_id = 235;

  ASSERT_EQ(-36, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, message_from_the_past_in_the_edge_is_detected)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  uint8_t             seq_id = 255;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));
  ASSERT_EQ(-1, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, message_from_the_future_is_detected)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 1;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  seq_id += 4;

  ASSERT_EQ(3, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, message_from_the_future_in_the_edge_is_detected)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 254;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  seq_id = 0;

  ASSERT_EQ(1, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, message_from_the_future_is_detected_with_big_difference)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 234;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  seq_id = 30;

  ASSERT_EQ(51, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker, two_eaxc_with_different_seq_id_is_ok)
{
  sequence_id_checker checker;
  unsigned            eaxc_1   = 0;
  unsigned            seq_id_1 = 1;
  unsigned            eaxc_2   = 2;
  unsigned            seq_id_2 = 200;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_1, seq_id_1));
  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_2, seq_id_2));

  for (unsigned i = 0; i != 10; ++i) {
    ++seq_id_1;
    ++seq_id_2;

    ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_1, seq_id_1));
    ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_2, seq_id_2));
  }
}

TEST(ofh_sequence_id_checker, two_eaxc_with_different_seq_id_detects_past_message)
{
  sequence_id_checker checker;
  unsigned            eaxc_1   = 0;
  unsigned            seq_id_1 = 1;
  unsigned            eaxc_2   = 2;
  unsigned            seq_id_2 = 200;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_1, seq_id_1));
  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_2, seq_id_2));

  seq_id_1 = 250;
  seq_id_2 = 100;

  ASSERT_EQ(-8, checker.update_and_compare_seq_id(eaxc_1, seq_id_1));
  ASSERT_EQ(-101, checker.update_and_compare_seq_id(eaxc_2, seq_id_2));
}

TEST(ofh_sequence_id_checker, two_eaxc_with_different_seq_id_detects_future_message)
{
  sequence_id_checker checker;
  unsigned            eaxc_1   = 0;
  unsigned            seq_id_1 = 1;
  unsigned            eaxc_2   = 2;
  unsigned            seq_id_2 = 200;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_1, seq_id_1));
  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc_2, seq_id_2));

  seq_id_1 = 105;
  seq_id_2 = 15;

  ASSERT_EQ(103, checker.update_and_compare_seq_id(eaxc_1, seq_id_1));
  ASSERT_EQ(70, checker.update_and_compare_seq_id(eaxc_2, seq_id_2));
}

TEST(ofh_sequence_id_checker, message_from_the_past_does_not_modify_internal_seq_id)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 1;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  // Set a sequence identifier from the past.
  --seq_id;
  ASSERT_EQ(-2, checker.update_and_compare_seq_id(eaxc, seq_id));

  // Continue as if was normal. As it was a packet from the past, the expected sequence id does not change.
  ++seq_id;
  ASSERT_EQ(-1, checker.update_and_compare_seq_id(eaxc, seq_id));
}

TEST(ofh_sequence_id_checker,
     message_from_the_past_does_not_modify_internal_seq_id_and_will_resume_if_gets_to_the_expected_seq_id)
{
  sequence_id_checker checker;
  unsigned            eaxc   = 0;
  unsigned            seq_id = 1;

  ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));

  // Set a sequence identifier from the past.
  seq_id = 200;

  // Check that the expected sequence identifier is not modified.
  for (int i = 1; i != 58; ++i) {
    // Continue as if was normal. As it was a packet from the past, the expected sequence id does not change.
    ++seq_id;
    ASSERT_EQ((-58 + i), checker.update_and_compare_seq_id(eaxc, seq_id));
  }

  // Once the given sequence id matched the expected, increment the expected sequence id.
  for (int i = 0; i != 20; ++i) {
    ++seq_id;
    ASSERT_EQ(0, checker.update_and_compare_seq_id(eaxc, seq_id));
  }
}
