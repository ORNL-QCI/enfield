cmake_minimum_required (VERSION 3.13)

project (xacc-enfield LANGUAGES C CXX)
enable_testing()

set (CMAKE_STANDARD_REQUIRED ON)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if (NOT XACC_DIR)
  set(XACC_DIR "$ENV{HOME}/.xacc")
endif()

# Find Xacc
find_package(XACC REQUIRED)

set (CMAKE_MODULE_PATH  "${CMAKE_SOURCE_DIR}/cmake")
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
add_subdirectory (lib)

# Build the VOQC plugin for XACC
add_subdirectory(xacc)
