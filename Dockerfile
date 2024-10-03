# The Radiant Blockchain Developers
# The purpose of this image is to be able to host Radiant Node (RADN) and electrumx
# Build with: `docker build .`
# Public images at: https://hub.docker.com/repository/docker/radiantblockchain
FROM ubuntu:20.04

LABEL maintainer="radiantblockchain@protonmail.com"
LABEL version="1.0.0"
LABEL description="Docker image for radiantd node"

ARG DEBIAN_FRONTEND=nointeractive
RUN sed -i 's|http://archive.ubuntu.com/ubuntu|http://mirrors.edge.kernel.org/ubuntu|g' /etc/apt/sources.list
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

RUN apt update && apt install --no-install-recommends -y $PACKAGES && \
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
#RUN git clone --depth 1 --branch v1.3.0 https://github.com/radiantblockchain/radiant-node.git
COPY . /root/radiant-node
RUN mkdir /root/radiant-node/build
WORKDIR /root/radiant-node/build
RUN cmake -GNinja .. -DBUILD_RADIANT_QT=OFF

RUN apt-get update
RUN apt-get install -y dos2unix

# Ensure scripts have correct line endings and permissions
RUN find /root/radiant-node/build /root/radiant-node/cmake/utils /root/radiant-node/share -type f \( -name "*.sh" -o -name "*.py" \) -exec dos2unix {} \; -exec chmod +x {} \;

RUN ninja
RUN ninja install

WORKDIR /root/radiant-node/build/src

# Runtime environment variables from the .env file
ENV RPC_USER=${RPC_USER}
ENV RPC_PASSWORD=${RPC_PASSWORD}
ENV RPC_WORKQUEUE=${RPC_WORKQUEUE}
ENV RPC_THREADS=${RPC_THREADS}
ENV RPC_ALLOW_IP=${RPC_ALLOW_IP}
ENV TX_INDEX=${TX_INDEX}

EXPOSE 7332 7333

ENTRYPOINT ["radiantd", "-rpcworkqueue=${RPC_WORKQUEUE}", "-rpcthreads=${RPC_THREADS}", "-rest", "-server", "-rpcallowip=${RPC_ALLOW_IP}", "-txindex=${TX_INDEX}", "-rpcuser=${RPC_USER}", "-rpcpassword=${RPC_PASSWORD}"]
