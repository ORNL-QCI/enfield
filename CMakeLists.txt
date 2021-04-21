cmake_minimum_required (VERSION 3.13)

project (xacc-enfield LANGUAGES C CXX)
enable_testing()

set (CMAKE_STANDARD_REQUIRED ON)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set (CMAKE_MODULE_PATH  "${CMAKE_SOURCE_DIR}/cmake")
if (NOT JSONCPP_ROOT)
  set(JSONCPP_ROOT "$ENV{HOME}/.jsoncpp")
endif()

# Find JsonCpp
find_package(JsonCpp REQUIRED)
# Build Enfield Libs
include_directories(${JSONCPP_INCLUDE})
include_directories (include)
# Additional option to specify Flex Include directory
# In some cases, the Flex executable is not associated w/ an include directory.
if (FLEX_HOME)
    include_directories("${FLEX_HOME}")
endif()

option(XACC_ENABLE_EFD_INFO_LOG "Info logging" OFF)
if (XACC_ENABLE_EFD_INFO_LOG)
  add_definitions(-DXACC_ENABLE_EFD_INFO_LOG)
endif()

add_subdirectory (lib)
add_subdirectory (executor)
add_subdirectory (xacc)


