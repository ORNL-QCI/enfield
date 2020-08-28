add_library (EfdBMTImpl
    DefaultBMTQAllocatorImpl.cpp
    ImprovedBMTQAllocatorImpl.cpp)
target_compile_options(EfdBMTImpl PRIVATE -fPIC)
