Dependencies
============

These are the dependencies currently used by Bitcoin Cash Node. You can find instructions for installing them in the `build-*.md` file for your platform.

These dependencies are required:

| Dependency | Version used | Minimum required | CVEs | Shared | [Bundled Qt library](https://doc.qt.io/qt-5/configure-options.html) | Purpose | Description |
| --- | --- | --- | --- | --- | --- |--- | --- |
| Berkeley DB | [5.3.28](http://www.oracle.com/technetwork/database/database-technologies/berkeleydb/downloads/index.html) | 5.3 | No |  |  | Wallet storage | Only needed when wallet enabled  |
| Boost | [1.70.0](http://www.boost.org/users/download/) | 1.58.0 | No |  |  |  Utility          | Library for threading, data structures, etc
| Clang |  | [3.4](http://llvm.org/releases/download.html) (C++14 support) |  |  |  |  |  |
| CMake |  | [3.13](https://cmake.org/download/) |  |  |  |  |  |
| D-Bus | [1.10.18](https://cgit.freedesktop.org/dbus/dbus/tree/NEWS?h=dbus-1.10) |  | No | Yes |  |  |  |
| Expat | [2.2.5](https://libexpat.github.io/) |  | No | Yes |  |  |  |
| fontconfig | [2.12.6](https://www.freedesktop.org/software/fontconfig/release/) |  | No | Yes |  |  |  |
| FreeType | [2.7.1](http://download.savannah.gnu.org/releases/freetype) |  | No |  |  |  |  |
| GCC |  | [5.0](https://gcc.gnu.org/) (C++14 support) |  |  |  |  |  |
| HarfBuzz-NG |  |  |  |  |  |  |  |
| jemalloc | [5.2.1](https://github.com/jemalloc/jemalloc/releases) |  |  |  |  |
| libevent | [2.1.8-stable](https://github.com/libevent/libevent/releases) | 2.0.22 | No |  |  |  Networking       | OS independent asynchronous networking |
| libjpeg |  |  |  |  | Yes |  |  |
| libpng |  |  |  |  | Yes |  |  |
| MiniUPnPc | [2.0.20180203](http://miniupnp.free.fr/files) | 1.5 | No |  |  | UPnP Support     | Firewall-jumping support |
| Ninja |  | [1.5.1](https://github.com/ninja-build/ninja/releases) |  |  |  |  |  |
| OpenSSL | [1.0.1k](https://www.openssl.org/source) |  | Yes |  |  | Crypto | Random Number Generation, Elliptic Curve Cryptography
| PCRE |  |  |  |  | Yes |  |  |
| protobuf | [2.6.1](https://github.com/google/protobuf/releases) |  | No |  |  |  Payments in GUI  | Data interchange format used for payment protocol (only needed when BIP70 enabled)
| Python (tests) |  | [3.5](https://www.python.org/downloads) |  |  |  |  |  |
| qrencode | [3.4.4](https://fukuchi.org/works/qrencode) |  | No |  |  | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)
| Qt | [5.9.6](https://download.qt.io/official_releases/qt/) | 5.5.1 | No |  |  |  GUI              | GUI toolkit (only needed when GUI enabled) |
| univalue |||||   | Utility          | JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
| XCB |  |  |  |  | Yes (Linux only) |  |  |
| xkbcommon |  |  |  |  | Yes (Linux only) |  |  |
| ZeroMQ | [4.1.5](https://github.com/zeromq/libzmq/releases) | 4.1.5 | No |  |  | ZMQ notification | Optional, allows generating ZMQ notifications (requires ZMQ version >= 4.1.5)
| zlib | [1.2.11](http://zlib.net/) |  |  |  | No |  |  |
