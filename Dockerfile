# The Radiant Blockchain Developers
# The purpose of this image is to be able to host Radiant Node (RADN) and electrumx
# Build with: `docker build .`
# Public images at: https://hub.docker.com/repository/docker/radiantblockchain
FROM ubuntu:20.04

LABEL maintainer="radiantblockchain@protonmail.com"
LABEL version="1.0.0"
LABEL description="Docker image for radiantd and electrumx"
ARG testvar=123453

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
  ocl-icd-* \
  opencl-headers \
  ocl-icd-opencl-dev\
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
"

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
RUN git clone https://github.com/radiantblockchain/radiant-node.git
RUN mkdir /root/radiant-node/build
WORKDIR /root/radiant-node/build
RUN cmake -GNinja .. -DBUILD_RADIANT_QT=OFF
RUN ninja
RUN ninja install

WORKDIR /root/radiant-node/build/src
ENTRYPOINT ["radiantd", "-rpcworkqueue=64", "-rpcthreads=64", "-rest", "-server", "-rpcbind","-rpcallowip='0.0.0.0/0'", "-txindex=1", "-rpcuser=raduser", "-rpcpassword=radpass"]
 
# Load up a shell to be able to run on vast.ai and connect
CMD ["sh"]

# Once you login you will see a folder structure like:
#
# root@C.482204:~$ ls
# onstart.sh  rad-bfgminer  rad-bfgminer-helper
# root@C.482204:~$ 
#
# Launch rad-bfgminer like (specify url to the radiantd node process or a stratum server)
#
# /root/rad-bfgminer/bfgminer -S opencl:auto -o http://127.0.0.1:7332 -u raduser -p radpass --set-device OCL:kernel=poclbm --coinbase-sig rad-bfgminer-misc --generate-to 16JR3uTBpTSnhWfLdX8D5EcMrTVhrBCr2X
# /root/rad-bfgminer/bfgminer -S opencl:auto -o http://miner.radiantblockchain.org:7332 -u raduser -p radpass --set-device OCL:kernel=poclbm --coinbase-sig rad-bfgminer-misc --generate-to 16JR3uTBpTSnhWfLdX8D5EcMrTVhrBCr2X >> /root/bfgminer.txt 
# /root/rad-bfgminer/bfgminer -S opencl:auto -o http://miner.radiantblockchain.org:7332 -u raduser -p radpass --set-device OCL0:kernel=poclbm --set-device OCL1:kernel=poclbm --set-device OCL2:kernel=poclbm --set-device OCL3:kernel=poclbm --coinbase-sig rad-bfgminer-misc --generate-to 16JR3uTBpTSnhWfLdX8D5EcMrTVhrBCr2X
# The above command will begin mining and generating any coinbase rewards into 1KSFaegQYMgQRfr2jWfHxy5pv6CQvHB5Lz
#
#
# ------ Optional ------
# To launch the rad-bfgminer with the key rotation, prepare the following
#
# Step 1. First generate a 12 word seed phrase with any program OR just run the miner the first time:
#
# node /root/rad-bfgminer-helper/start_radbfgminer.js
# 
# node start_radbfgminer.js          
#----------------------------------------------------
# Mnemonic seed phrase random every time (NOT USED) --> this success manual property energy cry feel shift celery not valid no bullet <--
# Showing first address associated with the random mnemonic seed phrase... 
# Address:  13rXNRH2afHmhGdWGZfz9KMusfRQ7TVxaz
# Private Key WIF:  KxK2f6eKvRcHsoFjmWDQkkANXpmp1Bi4Qhhdbj1k8Yhkn2pA
# xpriv:  xprv9s21ZrQH143K3EwsTqYrVHPeVCEgWdEsPPxieU1CGmN7ChpEkFVqrfnF5hHgAn7LfUxvzSX4u79Zwjwp5MVGcJEBSpjJNtuF8vy4cuFbs
# xpub:  xpub661MyMwAqRbcFj2LZs5rzRLf3E5Av5xikctKQqPXokEuXrMn4W3eB9dWP9QvRy65oSAZdQL9p43pHgJreMXoVWsceJcRpmSAQSwdsTBGk
#----------------------------------------------------
#
# Notice how it generates a new 12 words above (ex: dance sauce drill olive story swamp strategy lion jungle mass rib try)
# 
# Each time the program runs it produces a new random 12 words for convenience.
#
# Step 2. Copy the 12 word seed phrase into the file at rad-bfgminer-helper/.env into the PHRASE variable:
#
# PHRASE="REPLACE BETWEEN THE QUOTES HERE THE 12 WORD SEED PHRASE HERE"
# RPCUSER=raduser
# RPCPASS=radpass
# RPCHOSTPORT=node.radiantone.org:7332
# BFGMINERPATH=/root/rad-bfgminer
#
# Optionally you can replace the RPCHOSTPORT with your own radiantd node process or a stratum pool.
#
# Step 3. The .env file is now configured and you can run the miner again to start mining into addresses generated
# from your 12 word seed phrase
#
# node ./start_radbfgminer.js
#
# You will see output that the GPU miner starts up and every few seconds it will output something like:
#
# Checking if miner process is still alive... Sun Jun 12 2022 16:57:49 GMT+0000 (Coordinated Universal Time) ...
# ...Yes, it is alive!
#
# Everytime it finds a block it terminates the process and re-spawns with the next address
#
# Look inside rad-bfgminer-helper/minerIndex.json and see which key index is being used (it will increment on each new found block)

 