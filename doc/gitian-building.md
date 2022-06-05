# Gitian building

*Setup instructions for a Gitian build of Radiant Node using a VM or
physical system.*

Gitian is the deterministic build process that is used to build the Bitcoin
Cash Node executables. It provides a way to be reasonably sure that the
executables are really built from the source on GitLab/Github. It also makes sure
that the same, tested dependencies are used and statically built into the executable.

Multiple developers build the source code by following a specific descriptor
("recipe"), cryptographically sign the result, and upload the resulting signature.
These results are compared and only if they match, the build is accepted and
uploaded to radiantblockchain.org.

More independent Gitian builders are needed, which is why this guide exists.
It is preferred you follow these steps yourself instead of using someone else's
VM image to avoid 'contaminating' the build.

If you are running Debian, Ubuntu or Mac, see
[Running Gitian with Docker](./gitian-building/gitian-building-docker.md)
for a relatively straightforward setup and build process.

The instructions below use the automated script [gitian-build.py](../contrib/gitian-build.py)
which only works in Debian/Ubuntu. For manual steps and instructions for fully
offline signing, see [this guide](./gitian-building/gitian-building-manual.md).

## Preparing the Gitian builder host

The first step is to prepare the host environment that will be used to perform
the Gitian builds. This guide explains how to set up the environment, and how to
start the builds.

Gitian builds are known to be working on recent versions of Debian and Ubuntu.
If your machine is already running one of those operating systems, you can
perform Gitian builds on the actual hardware.
Alternatively, you can install one of the supported operating systems in a virtual
machine.

You can create the virtual machine using [vagrant](./gitian-building/gitian-building-vagrant.md)
or chose to setup the VM manually.

Any kind of virtualization can be used, for example:

* [VirtualBox](https://www.virtualbox.org/) (covered by this guide)
* [KVM](http://www.linux-kvm.org/page/Main_Page)
* [LXC](https://linuxcontainers.org/)

Please refer to the following documents to set up the operating systems and Gitian.

* (optional) To setup a Debian virtual machine see [Create Debian VirtualBox](./gitian-building/gitian-building-create-vm-debian.md)
* To setup Gitian on your new Debian VM see [Setup Gitian on Debian](./gitian-building/gitian-building-setup-gitian-debian.md)

Note that a version of `lxc-execute` higher or equal to 2.1.1 is required.
You can check the version with `lxc-execute --version`.

## MacOS code signing

In order to sign builds for MacOS, you need to obtain an archive which has been
extracted from the free SDK.

```bash
cd ~/gitian-builder
curl -LO https://github.com/phracker/MacOSX-SDKs/releases/download/10.15/MacOSX10.14.sdk.tar.xz
echo "0f03869f72df8705b832910517b47dd5b79eb4e160512602f593ed243b28715f MacOSX10.14.sdk.tar.xz" | sha256sum -c
# Should echo "MacOSX10.14.sdk.tar.xz: OK"
mkdir -p inputs
mv MacOSX10.14.sdk.tar.xz inputs
```

Alternatively, you can skip the macOS build by adding `--os=lw` below.

The `gitian-build.py` script will checkout different release tags, so it's best
to copy it:

```bash
cp radiant-node/contrib/gitian-build.py .
```

You only need to do this once:

```bash
./gitian-build.py --setup satoshi 22.2.0
```

Where `satoshi` is your GitLab name and `22.2.0` represents the most recent tag
(without `v`) - use the latest released version available.

## Build binaries

Windows and macOS have code signed binaries, but those won't be available until
a few developers have gitian signed the non-codesigned binaries.

To build the most recent tag:

```bash
./gitian-build.py --detach-sign --no-commit -b satoshi 22.2.0
```

If running in a VM, to speed up the build use `-j 5 -m 5000` as the first
arguments, where `5` is the number of CPU's you allocated to the VM plus one,
and 5000 is a little bit less than then the MB's of RAM you allocated.

## Verify hashes

If all went well, this prints a report with hashes for each asset produced.
E.g.:

```
[...]
Generating report
4dcfd0b27ff21736483f22aba1f6c4bd8845f0b2582f2f6c4314bd90488dddd1  radiant-node-23.1.0-win-unsigned.tar.gz
1c2e737651b69f46cb2ab4e28d8da34168274d526330569c1b2f4d0dff4e11da  radiant-node-23.1.0-win64-debug.zip
8f68a567d257107ba33c494c49e151d38678891de853eddc0ad2f391ae1c254e  radiant-node-23.1.0-win64-setup-unsigned.exe
c73136187dcbbf124088029bd65e6303118563d1d04c5fca464f9492cd365e69  radiant-node-23.1.0-win64.zip
0356cf228afb7da3bf945198f27fdadd972e1c5e5f6df57d863bb4c16e2836e0  src/radiant-node-23.1.0.tar.gz
```

Confirm with other developers that the hashes match with theirs.

Lastly, follow the instructions at [Gitian signing](./gitian-signing.md) if you
need to sign your builds too.
