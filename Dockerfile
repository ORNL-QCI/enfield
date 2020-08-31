from xacc/ubuntu:18.04
# Clone and install XACC
run git clone --recursive https://github.com/eclipse/xacc && cd xacc && mkdir build && cd build \
    && cmake .. -DXACC_BUILD_TESTS=TRUE -DXACC_BUILD_EXAMPLES=TRUE \
    && make -j$(nproc) install && cd ../..
# Install JsonCpp
run git clone https://github.com/open-source-parsers/jsoncpp.git && cd jsoncpp && mkdir build && cd build \
    && cmake .. -DCMAKE_INSTALL_PREFIX=~/.jsoncpp \
    && make -j$(nproc) install && cd ../..
# Install Flex and Bison
run apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends bison libfl-dev 
    


