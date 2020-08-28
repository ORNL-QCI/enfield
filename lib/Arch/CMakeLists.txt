add_library (EfdArch
    ArchGraph.cpp
    Architectures.cpp)
target_compile_options(EfdArch PRIVATE -fPIC)
