cmake_minimum_required(VERSION 3.16)
project(stm32f407_can_uds_iap C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_library(iap_core STATIC src/crc32.c src/image.c src/updater.c)
target_include_directories(iap_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(iap_protocol STATIC protocol/src/isotp.c protocol/src/uds_boot.c protocol/src/reply_gate.c)
target_include_directories(iap_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/protocol/include)
target_link_libraries(iap_protocol PUBLIC iap_core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(iap_core PRIVATE -Wall -Wextra -Werror -Wpedantic)
  target_compile_options(iap_protocol PRIVATE -Wall -Wextra -Werror -Wpedantic)
endif()
include(CTest)
if(BUILD_TESTING)
  add_executable(iap_core_test tests/test_boot_core.c tests/mock_flash.c)
  target_link_libraries(iap_core_test PRIVATE iap_core)
  target_include_directories(iap_core_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

  add_executable(stm32f407_flash_layout_test
    tests/test_flash_layout.c ports/stm32f407/src/flash_layout.c)
  target_include_directories(stm32f407_flash_layout_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/ports/stm32f407/include)

  add_executable(m2_protocol_test tests/test_m2_protocol.c tests/mock_flash.c)
  target_link_libraries(m2_protocol_test PRIVATE iap_protocol)
  target_include_directories(m2_protocol_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

  add_executable(handoff_policy_test tests/test_handoff_policy.c)
  target_include_directories(handoff_policy_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ports/stm32f407/include)

  add_executable(reply_gate_test tests/test_reply_gate.c)
  target_link_libraries(reply_gate_test PRIVATE iap_protocol)

  add_executable(power_cut_matrix tests/test_power_cut_matrix.c tests/mock_flash.c)
  target_link_libraries(power_cut_matrix PRIVATE iap_core)
  target_include_directories(power_cut_matrix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_executable(can_frame_trace tests/test_can_frame_trace.c tests/mock_flash.c)
  target_link_libraries(can_frame_trace PRIVATE iap_protocol)
  target_include_directories(can_frame_trace PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME power_cut_matrix COMMAND power_cut_matrix)
  add_test(NAME can_frame_trace COMMAND can_frame_trace)
  add_executable(iap_uds_bridge tests/uds_bridge.c tests/mock_flash.c)
  target_link_libraries(iap_uds_bridge PRIVATE iap_protocol)
  target_include_directories(iap_uds_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(t iap_core_test stm32f407_flash_layout_test m2_protocol_test handoff_policy_test reply_gate_test power_cut_matrix can_frame_trace iap_uds_bridge)
      target_compile_options(${t} PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endforeach()
  endif()
  add_test(NAME boot_core COMMAND iap_core_test)
  add_test(NAME flash_layout COMMAND stm32f407_flash_layout_test)
  add_test(NAME m2_protocol COMMAND m2_protocol_test)
  add_test(NAME handoff_policy COMMAND handoff_policy_test)
  add_test(NAME reply_gate COMMAND reply_gate_test)

  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME host_updater
    COMMAND ${Python3_EXECUTABLE} -m unittest discover
      -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_host_updater.py -v)
  set_tests_properties(host_updater PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    ENVIRONMENT "IAP_UDS_BRIDGE=$<TARGET_FILE:iap_uds_bridge>")
endif()
