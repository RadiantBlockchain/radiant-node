Radiant Node
=================

The goal of Radiant Node is to create sound money and a digital value transfer
system that is usable by everyone in the world. This is civilization-changing 
technology which will dramatically increase human flourishing, freedom, and 
prosperity. The project aims to achieve this goal by focusing on high performance
scalablity and an expressive programming language to realize any type of digital
value and money transfer imaginable.

What is Radiant?
---------------------

Radiant is a high performance blockchain for digital assets and enables instant 
payments and asset transfers to to anyone, anywhere in the world. It uses 
peer-to-peer technology to operate with no central authority: managing 
transactions are carried out collectively by the network. Radiant is a twin 
network based on the original Bitcoin design.

Quick Start Compile on Ubuntu 18/20
---------------------

See [Ubuntu/Debian Builds](build_unix.md) or the Docs folder for your platform

Disable QT when building:

```
mkdir build
cd build
cmake -GNinja .. -DBUILD_RADIANT_QT=OFF
ninja
# Optionally install into system
sudo ninja install 
```

Install on System Start with systemd
--------------------------
In: `/etc/systemd/system/radiantd.service `
```
[Unit]
Description=radiantd
After=network.target

[Service]
PIDFile=/tmp/radiantd-99.pid
ExecStart=/usr/local/bin/radiantd -rpcworkqueue=16 -rpcthreads=16  -rest -server -rpcallowip='0.0.0.0/0' -txindex=1 -rpcuser=raduser -rpcpassword=radpass 

User=root
Group=root
Restart=always
LimitNOFILE=400000
TimeoutStopSec=30min

[Install]
WantedBy=multi-user.target

```

Sample radiant.conf
--------------------------
 
```
rpcallowip=0.0.0.0/0
txindex=1
rpcuser=youruser
rpcpassword=yourpassword

```


What is Radiant Node?
--------------------------

[Radiant Node](https://radiantblockchain.org) is the name of open-source
software which enables the use of Radiant. It is a descendant of the 
[Bitcoin Cash Node](https://bitcoincashnode.org) [Bitcoin Core](https://bitcoincore.org) 
and [Bitcoin ABC](https://www.bitcoinabc.org)
software projects.

License
-------

Radiant Node is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or see
[https://opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).

This product includes software developed by the OpenSSL Project for use in the
[OpenSSL Toolkit](https://www.openssl.org/), cryptographic software written by
[Eric Young](mailto:eay@cryptsoft.com), and UPnP software written by Thomas
Bernard.

Development Process
-------------------

Radiant Node development takes place at [https://gitlab.com/radiantblockchain/radiant-node](https://gitlab.com/radiantblockchain/radiant-node)

This Github repository contains only source code of releases.

Disclosure Policy
-----------------

We have a [Disclosure Policy](DISCLOSURE_POLICY.md) for responsible disclosure
of security issues.

Further info
------------

See [doc/README.md](doc/README.md) for further info on installation, building,
development and more.
