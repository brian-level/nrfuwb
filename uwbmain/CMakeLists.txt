
cmake_minimum_required(VERSION 3.20.0)

set(PROJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(TREE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMPONENTS_DIR ${TREE_ROOT}/components)
set(DRIVERS_DIR ${TREE_ROOT}/drivers)
set(APPLICATION_DIR ${PROJ_ROOT}/src)
set(BIXBY_DIR ${TREE_ROOT}/Bixby/Source)

# include our common cmake functions
include(${TREE_ROOT}/helpers.cmake)

# these 2 enable a remarkably sketchy hack that lets us mix s/w implementation
# of secpk1_256 curves while retaining h/w impl. of other curves
# (since k1 isnt handled in h/w atm)
add_compile_definitions(LEVEL_ECP_ALT_PER_CURVE)
add_compile_definitions(MBEDTLS_ECP_DP_SECP256K1_ENABLED)

list(APPEND OVERLAY_CONFIG "${PROJ_ROOT}/prj.conf")
list(APPEND OVERLAY_CONFIG "${PROJ_ROOT}/prj_${BOARD}.conf")

if(EXISTS "${PROJ_ROOT}/child_image/mcuboot/mcuboot_${BOARD}.conf")
    set(mcuboot_CONF_FILE $ENV{ZEPHYR_BASE}/../bootloader/mcuboot/boot/zephyr/prj.conf ${PROJ_ROOT}/child_image/mcuboot/mcuboot_${BOARD}.conf)
    set(mcuboot_DTC_OVERLAY_FILE ${BOARD_ROOT}/boards/${BOARD}.overlay)
endif()

set(DTC_OVERLAY_FILE ${BOARD_ROOT}/boards/${BOARD}.overlay)

list(APPEND DTS_ROOT ${TREE_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(artemis_ouer)

# these two turn on assert-logging and "require" macro failure logging
# and take up a good chunk of space in code
add_compile_definitions(stargate_ftd) #  force LoggingConfig.h to use LOG_INF semantics for ncs
add_compile_definitions(INTERNAL)
add_compile_definitions(BUILT_WITH_CMAKE)

add_compile_definitions(APP_NAME_STR="nrfuwb")

set(APPLICATION_SRCS
  ${APPLICATION_DIR}/main.c
)

add_level_component(uwb)
add_level_component(uci)
add_level_component(hbci)
add_level_component(nrfspi)
add_level_component(timesvc)
add_level_component(crypto)

if(CONFIG_BT)
  add_level_component(ble)
endif()

target_sources(app PRIVATE
  ${APPLICATION_SRCS}
)

target_include_directories(app PRIVATE
  ${APPLICATION_DIR}
  ${BIXBY_DIR}/Include
)

# Wait for the MCUBoot Image to generate the public key and then pull that into parent image
#set(GENERATED_PUBKEY ${ZEPHYR_BINARY_DIR}/../mcuboot/zephyr/autogen-pubkey.c)
#add_custom_command(
#  OUTPUT ${GENERATED_PUBKEY}
#  COMMAND cat ${GENERATED_PUBKEY}
#  DEPENDS mcuboot_subimage
#)
#set_property(TARGET app APPEND PROPERTY SOURCES ${GENERATED_PUBKEY})

