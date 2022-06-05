# Running Gitian with Docker

This is a streamlined guide for running Gitian builds with Docker on Ubuntu,
Debian or Mac hardware.

## Setup

Ensure you have Docker installed.  See <https://docs.docker.com/get-docker/> for
installation instructions.

If you're using Linux, make sure to go through the Linux post-install
walkthrough, especially the
[Manage Docker as a non-root user](https://docs.docker.com/engine/install/linux-postinstall/#manage-docker-as-a-non-root-user)
section, to avoid having to use `sudo` all the time.

### Prepare a build workspace

Create a workspace directory (e.g. `~/radn-gitian`) and `cd` into it.  You'll
only need to run through this setup once so long as you retain the workspace.

### Install dependencies

You'll need `git` and `curl` installed.  If on Linux, just do `sudo apt install
curl git`.

### Gitian setup

```bash
# Fetch the `gitian-build.py` script
git clone https://github.com/radiantblockchain/radiant-node.git
cp radiant-node/contrib/gitian-build.py .

# If you are on a MacOS host, you will need the MacOS-capable fork
# of gitian-builder, if on Linux, you can skip this step.
git clone -b macos_support_no_debian_cache https://github.com/cculianu/gitian-builder.git

# Run the initial Gitian setup
./gitian-build.py --docker --setup

# If you need to build for MacOS, also fetch this archive which has been
# extracted from the free SDK.
mkdir -p gitian-builder/inputs
(cd gitian-builder/inputs
curl -LO https://github.com/phracker/MacOSX-SDKs/releases/download/10.15/MacOSX10.14.sdk.tar.xz
echo "0f03869f72df8705b832910517b47dd5b79eb4e160512602f593ed243b28715f MacOSX10.14.sdk.tar.xz" | sha256sum -c)
# This should echo "MacOSX10.14.sdk.tar.xz: OK"
```

## Build binaries

Finally, use the following command to run the build process. Replace `satoshi`
with your GitLab name and replace `23.1.0` with the most recent tag
(without the "v"). Use the latest released version available.

```bash
./gitian-build.py --docker --detach-sign --no-commit -b satoshi 23.1.0
```

See the [Verify hashes](../gitian-building.md#verify-hashes) section of the
main Gitian build guide for build verification and signing instructions.
