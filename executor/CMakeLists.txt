add_library(efd EnfieldExecutor.cpp)
target_link_libraries (efd
    EfdArch EfdAllocator EfdBMTImpl EfdSimpleImpl
    EfdTransform EfdAnalysis EfdSupport
    ${JSONCPP_MAIN})