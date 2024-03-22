# The Radiant Blockchain Developers
# The purpose of this image is to be able to host Radiant Node (RADN) and electrumx
# Build with: `docker build .`
# Public images at: https://hub.docker.com/repository/docker/radiantblockchain
FROM ubuntu:20.04

LABEL maintainer="radiantblockchain@protonmail.com"
LABEL version="1.0.0"
LABEL description="Docker image for radiantd node"

ARG DEBIAN_FRONTEND=nointeractive
RUN apt update
RUN apt-get install -y curl
RUN curl -sL https://deb.nodesource.com/setup_12.x | bash -
RUN apt-get install -y nodejs

ENV PACKAGES="\
  build-essential \
  libcurl4-openssl-dev \
  software-properties-common \
  ubuntu-drivers-common \
  pkg-config \
  libtool \
  openssh-server \
  git \
  clinfo \
  autoconf \
  automake \
  libjansson-dev \
  libevent-dev \
  uthash-dev \
  nodejs \
  vim \
  libboost-chrono-dev \
  libboost-filesystem-dev \
  libboost-test-dev \
  libboost-thread-dev \
  libevent-dev \
  libminiupnpc-dev \
  libssl-dev \
  libzmq3-dev \ 
  help2man \
  ninja-build \
  python3 \
  libdb++-dev \
  wget \
  cmake \
  ocl-icd-* \
  opencl-headers \
  ocl-icd-opencl-dev\
"
# Note can remove the opencl and ocl packages above when not building on a system for GPU/mining
# Included only for reference purposes if this container would be used for mining as well.

RUN apt update && apt install --no-install-recommends -y $PACKAGES  && \
    rm -rf /var/lib/apt/lists/* && \
    apt clean

# Install cmake to prepare for radiant-node
# RUN mkdir /root/cmaketmp
# WORKDIR /root/cmaketmp
# RUN wget https://github.com/Kitware/CMake/releases/download/v3.20.0/cmake-3.20.0.tar.gz
# RUN tar -zxvf cmake-3.20.0.tar.gz
# WORKDIR /root/cmake-3.20.0
# RUN ./bootstrap
# RUN make
# RUN make install
 
# Install radiant-node
WORKDIR /root
RUN git clone --depth 1 --branch v1.3.0 https://github.com/radiantblockchain/radiant-node.git
RUN mkdir /root/radiant-node/build
WORKDIR /root/radiant-node/build
RUN cmake -GNinja .. -DBUILD_RADIANT_QT=OFF
RUN ninja
RUN ninja install

WORKDIR /root/radiant-node/build/src

EXPOSE 7332 7333
 
ENTRYPOINT ["radiantd", "-rpcworkqueue=32", "-rpcthreads=16", "-rest", "-server", "-rpcallowip=0.0.0.0/0",  "-txindex=1", "-rpcuser=dockeruser", "-rpcpassword=dockerpass"]
  
 
