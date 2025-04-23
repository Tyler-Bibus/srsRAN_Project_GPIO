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

add_executable(pss_processor_test pss_processor_test.cpp)
target_link_libraries(pss_processor_test srsran_signal_processors srsvec srslog)
add_test(pss_processor_test pss_processor_test)

add_executable(sss_processor_test sss_processor_test.cpp)
target_link_libraries(sss_processor_test srsran_signal_processors srsvec srslog)
add_test(sss_processor_test sss_processor_test)

if (USE_PHY_TESTVECTORS)
endif (USE_PHY_TESTVECTORS)
