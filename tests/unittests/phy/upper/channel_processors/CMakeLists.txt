#
# Copyright 2021-2023 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#
add_subdirectory(pusch)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_data)

set_directory_properties(PROPERTIES LABELS "phy")

add_executable(pdcch_processor_unittest pdcch_processor_unittest.cpp)
target_link_libraries(pdcch_processor_unittest srsran_channel_processors srslog)
add_test(pdcch_processor_unittest pdcch_processor_unittest)

add_executable(pdsch_processor_unittest pdsch_processor_unittest.cpp)
target_link_libraries(pdsch_processor_unittest srsran_channel_processors srslog gtest gtest_main)
add_test(pdsch_processor_unittest pdsch_processor_unittest)

add_executable(pdsch_processor_validator_test pdsch_processor_validator_test.cpp)
target_link_libraries(pdsch_processor_validator_test
        srsran_channel_equalizer
        srsran_channel_processors
        srsran_phy_support
        srslog
        srsran_upper_phy_support
       
        gtest
        gtest_main)
add_test(pdsch_processor_validator_test pdsch_processor_validator_test)

add_executable(pucch_processor_format1_unittest pucch_processor_format1_unittest.cpp)
target_link_libraries(pucch_processor_format1_unittest
        srsran_channel_equalizer
        srsran_channel_processors
        srslog
        gtest
        gtest_main)
add_test(pucch_processor_format1_unittest pucch_processor_format1_unittest)

add_executable(pucch_processor_validator_format2_test pucch_processor_validator_format2_test.cpp)
target_link_libraries(pucch_processor_validator_format2_test
        srsran_channel_equalizer
        srsran_channel_processors
        srsran_phy_support
        srslog
        srsran_upper_phy_support
        gtest
        gtest_main)
add_test(pucch_processor_validator_format2_test pucch_processor_validator_format2_test)

add_executable(pucch_processor_validator_format1_test pucch_processor_validator_format1_test.cpp)
target_link_libraries(pucch_processor_validator_format1_test
        srsran_channel_equalizer
        srsran_channel_processors
        srsran_phy_support
        srslog
        srsran_upper_phy_support
        gtest
        gtest_main)
add_test(pucch_processor_validator_format1_test pucch_processor_validator_format1_test)

add_executable(ssb_processor_unittest ssb_processor_unittest.cpp)
target_link_libraries(ssb_processor_unittest srsran_channel_processors srslog)
add_test(ssb_processor_unittest ssb_processor_unittest)

if (USE_PHY_TESTVECTORS)
endif (USE_PHY_TESTVECTORS)
