#
# Copyright 2021-2023 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_data)

set_directory_properties(PROPERTIES LABELS "phy")

add_executable(pusch_processor_validator_test pusch_processor_validator_test.cpp)
target_link_libraries(pusch_processor_validator_test
        srsran_channel_equalizer
        srsran_channel_processors
        srsran_phy_support
        srslog
        srsran_upper_phy_support
        gtest
        gtest_main)
add_test(pusch_processor_validator_test pusch_processor_validator_test)

add_executable(pusch_processor_unittest pusch_processor_unittest.cpp)
target_link_libraries(pusch_processor_unittest srsran_channel_processors srslog gtest gtest_main)
add_test(pusch_processor_unittest pusch_processor_unittest)

if (USE_PHY_TESTVECTORS)
endif (USE_PHY_TESTVECTORS)
