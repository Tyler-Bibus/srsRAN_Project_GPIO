#
# Copyright 2021-2025 Software Radio Systems Limited
#
# This file is part of srsRAN
#
# srsRAN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsRAN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

find_package(SCTP REQUIRED)

add_library(srsran_gateway
        sctp_network_gateway_common_impl.cpp
        sctp_network_client_impl.cpp
        sctp_network_server_impl.cpp
        sctp_network_gateway_impl.cpp
        udp_network_gateway_impl.cpp
        sctp_network_gateway_factory.cpp
        sctp_network_client_factory.cpp
        sctp_network_server_factory.cpp
        udp_network_gateway_factory.cpp
)
target_link_libraries(srsran_gateway srsran_support srslog ${SCTP_LIBRARIES})
