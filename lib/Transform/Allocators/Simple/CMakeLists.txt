add_library (EfdSimpleImpl
    IdentityMappingFinder.cpp
    PathGuidedSolBuilder.cpp
    QbitterSolBuilder.cpp
    RandomMappingFinder.cpp
    WeightedSIMappingFinder.cpp)

target_compile_options(EfdSimpleImpl PRIVATE -fPIC)
