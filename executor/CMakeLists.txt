set (LIBRARY_NAME efd) 
add_library(${LIBRARY_NAME} STATIC EnfieldExecutor.cpp)
target_compile_options(${LIBRARY_NAME} PRIVATE -fPIC)
target_link_libraries (efd
    EfdArch EfdAllocator EfdBMTImpl EfdSimpleImpl
    EfdTransform EfdAnalysis EfdSupport
    ${JSONCPP_MAIN})