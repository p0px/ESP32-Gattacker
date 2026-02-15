# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Retrieve version from git
find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(GIT_VERSION "v0.0.0-unknown")
endif()

if("${GIT_VERSION}" STREQUAL "")
  set(GIT_VERSION "v0.0.0-unknown")
endif()

message(STATUS "Project Version: ${GIT_VERSION}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/main/version.h.in"
  "${CMAKE_BINARY_DIR}/config/version.h"
)

# Add version.h to include path
include_directories(${CMAKE_BINARY_DIR}/config)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gattacker)
littlefs_create_partition_image(spiffs data FLASH_IN_PROJECT)

# Custom target to merge binaries
set(RELEASE_BIN_NAME "esp-gattacker-${GIT_VERSION}.bin")

add_custom_command(TARGET app POST_BUILD
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/merge_firmware.py
    --output ${CMAKE_BINARY_DIR}/${RELEASE_BIN_NAME}
    --bootloader ${CMAKE_BINARY_DIR}/bootloader/bootloader.bin
    --partition-table ${CMAKE_BINARY_DIR}/partition_table/partition-table.bin
    --ota ${CMAKE_BINARY_DIR}/ota_data_initial.bin
    --app ${CMAKE_BINARY_DIR}/gattacker.bin
    --spiffs ${CMAKE_BINARY_DIR}/spiffs.bin
  COMMENT "Merging binaries into ${RELEASE_BIN_NAME}..."
)