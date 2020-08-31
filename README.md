# XACC - Enfield Placement plugin

This branch implements an XACC placement plugin (`IRTransformation`)
based on the Enfield library.

## Build Instructions:

- Build the `Dockerfile` container. The `Dockerfile` will install XACC and other required libraries.

- Perform a standard build, i.e.

```
mkdir build && cd build
cmake ..
make install
```

- Run the unit test to veify the plugin if neeed:

```
ctest
```



