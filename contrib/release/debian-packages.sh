#!/usr/bin/env bash

export LC_ALL=C

set -euo pipefail

DEFAULT_PPA="bitcoin-cash-node"
DPUT_CONFIG_FILE=~/".dput.cf"
TOPLEVEL="$(git rev-parse --show-toplevel)"
KEYS_TXT="${TOPLEVEL}"/contrib/gitian-signing/keys.txt

help_message() {
  echo "Build and sign Debian packages and push to a PPA."
  echo "Usage: $0 <options> signer"
  echo
  echo "Example usage: $0 freetrader"
  echo
  echo "signer will be used to fetch the signing key fingerprint from '${KEYS_TXT}'"
  echo "  That matching fingerprint will be used to fetch the correctly formatted name and email from GPG."
  echo "  signer must at least partially match the fingerprint or email in keys.txt"
  echo
  echo "Note: This script will prompt you to sign with your PGP key."
  echo
  echo "-d, --dry-run               Build and sign the packages, but do not push them to the PPA."
  echo "-h, --help                  Display this help message."
  echo "-p, --ppa <ppa-name>        PPA hostname. Defaults to: '${DEFAULT_PPA}'. If no config file exists at ${DPUT_CONFIG_FILE}"
  echo "                              then one will be created using '${DEFAULT_PPA}'. Setting this option to a hostname other than"
  echo "                              the default will require that you add the necessary settings to the config file."
  echo "-v, --version <version>     Set the package version. Defaults to the version as stated in CMakeLists.txt."
  echo "                              If set, version must be of the form: MAJOR.MINOR.REVISION[.OPTIONALPATCH]"
  echo "                              OPTIONALPATCH may be necessary when source files have changed but the version revision has not,"
  echo "                              as the PPA will reject source archives of the same name."
  echo "-D, --deb-version <version> Set the package debian/ubuntu version. By default the value would be '1'. This version would be"
  echo "                              appended to the upstream version and the name of the distro (as defined by the debian packaging policies"
  echo "                              related to package versioning, for more details see:"
  echo "                              https://www.debian.org/doc/debian-policy/ch-controlfields.html#standards-version"
  echo "                              If set, version must be of the form of a single positive integer number."
  echo "                              This parameter be necessary when source files have not changed but the build of the"
  echo "                              packages failed for issues unrelated to the code base"
  echo "-u, --ubuntu-name <name>    A string parameter that specify the name of the ubuntu version we want to build the packages for. If not used"
  echo "                              the packages will be built for all the supported Ubuntu version. E.g. -u xenial."
}

DRY_RUN="false"
NUM_EXPECTED_ARGUMENTS=1
PACKAGE_VERSION=""
DEBIAN_PKG_VERSION=""
UBUNTU_NAME=""
PPA="${DEFAULT_PPA}"

# Parse command line arguments
while [[ $# -ne 0 ]]; do
case $1 in
  -d|--dry-run)
    DRY_RUN="true"
    shift # shift past argument
    ;;
  -h|--help)
    help_message
    exit 0
    ;;
  -p|--ppa)
    PPA="$2"
    shift # shift past argument
    shift # shift past value
    ;;
  -v|--version)
    PACKAGE_VERSION="$2"
    echo "${PACKAGE_VERSION}" | grep -E "[0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?" || {
      echo "Error: package_version is not formatted correctly"
      echo
      help_message
      exit 20
    }
    shift # shift past argument
    shift # shift past value
    ;;
  -D|--debian-version)
    DEBIAN_PKG_VERSION="$2"
    echo "${DEBIAN_PKG_VERSION}" | grep -E "[0-9]+?" || {
      echo "Error: debian_version is not formatted correctly"
      echo
      help_message
      exit 20
    }
    shift # shift past argument
    shift # shift past value
    ;;
  -u|--ubuntu-name)
    UBUNTU_NAME="$2"
    SUPPORTED_VERSIONS="xenial bionic eoan focal"
    echo "${SUPPORTED_VERSIONS}" | grep -qw ${UBUNTU_NAME} || {
      echo "$UBUNTU_NAME is not a valid or it is an unsupported Ubuntu version"
      echo "Supported versions are: ${SUPPORTED_VERSIONS}"
      exit 20
    }
    shift # shift past argument
    shift # shift past value
    ;;
  *)
    if [ "$#" -le "${NUM_EXPECTED_ARGUMENTS}" ]; then
      break
    fi
    echo "Unknown argument: $1"
    help_message
    exit 1
    ;;
esac
done

# Check for dependencies
if ! command -v dput > /dev/null; then
  echo "Error: 'dput' is not installed."
  exit 10
fi
if ! command -v debuild > /dev/null; then
  echo "Error: 'debuild' is not installed."
  exit 11
fi

if [ "$#" -ne "${NUM_EXPECTED_ARGUMENTS}" ]; then
  echo "Error: Expects ${NUM_EXPECTED_ARGUMENTS} arguments"
  echo
  help_message
  exit 20
fi

SIGNER_FINGERPRINT=$(grep "$1" "${KEYS_TXT}" | cut -d' ' -f 1) || {
  echo "Error: Signer '$1' does not match any line in '${KEYS_TXT}'"
  exit 21
}
NUM_FINGERPRINT_MATCHES=$(echo "${SIGNER_FINGERPRINT}" | wc -l)
if [ "${NUM_FINGERPRINT_MATCHES}" -ne 1 ]; then
  echo "Error: '$1' is expected to match only one line in '${KEYS_TXT}'. Got '${NUM_FINGERPRINT_MATCHES}'"
  exit 22
fi

SIGNER=$(gpg --list-key "${SIGNER_FINGERPRINT}" | grep -o "\[ultimate\] .* <.*@.*>" | cut -d' ' -f 2-)
echo "Signer: ${SIGNER}"
if [ -z "${SIGNER}" ]; then
  echo "Error: Signer key for '${SIGNER}' not found."
  exit 23
fi

# Generate default dput config file if none exists
if [ ! -f ${DPUT_CONFIG_FILE} ]; then
  echo "Info: No dput config file exists. Creating ${DPUT_CONFIG_FILE} now..."
  cat > ${DPUT_CONFIG_FILE} <<EOF
[${DEFAULT_PPA}]
fqdn = ppa.launchpad.net
method = ftp
incoming = ~bitcoin-cash-node/ubuntu/ppa/
login = anonymous
allow_unsigned_uploads = 0
EOF
fi

# Check that the requested PPA hostname exists
grep "\[${PPA}\]" ${DPUT_CONFIG_FILE} || {
  echo "Error: PPA hostname does not exist in ${DPUT_CONFIG_FILE}"
  exit 30
}

# Build package source archive
"${TOPLEVEL}"/contrib/release/configure_cmake.sh
pushd "${TOPLEVEL}"/build
ninja package_source

# Get package version if one wasn't explicitly set
if [ -z "${PACKAGE_VERSION}" ]; then
  PACKAGE_VERSION=$(grep VERSION ../CMakeLists.txt | grep -v required | awk '{print $2}')
fi
echo "Package version: ${PACKAGE_VERSION}"

# Get debian version if one wasn't explicitly set
if [ -z "${DEBIAN_PKG_VERSION}" ]; then
  DEBIAN_PKG_VERSION=1
fi
echo "Debian version: ${DEBIAN_PKG_VERSION}"
# Unpack the package source
SOURCE_VERSION=$(echo "${PACKAGE_VERSION}" | grep -oE "[0-9]+\.[0-9]+\.[0-9]+")
SOURCE_BASE_NAME="bitcoin-cash-node-${SOURCE_VERSION}"
SOURCE_ARCHIVE="${SOURCE_BASE_NAME}.tar.gz"
echo "tar -zxf ${SOURCE_ARCHIVE}"
tar -zxf "${SOURCE_ARCHIVE}"

# Rename the package source archive. debuild is picky about the naming.
CONTROL_SOURCE_NAME=$(grep "Source: " "${TOPLEVEL}"/contrib/debian/control | cut -c 9-)
PACKAGE_BASE_NAME="${CONTROL_SOURCE_NAME}_${PACKAGE_VERSION}"
PACKAGE_ARCHIVE="${PACKAGE_BASE_NAME}.orig.tar.gz"
echo "mv ${SOURCE_ARCHIVE}" "${PACKAGE_ARCHIVE}"
mv "${SOURCE_ARCHIVE}" "${PACKAGE_ARCHIVE}"

# Build package files for each supported distribution
DATE=$(date -R)
package() {
  DISTRO="$1"
  PACKAGE_NAME="${PACKAGE_BASE_NAME}-${DISTRO}${DEBIAN_PKG_VERSION}"

  pushd "${SOURCE_BASE_NAME}"
  cp -r "${TOPLEVEL}"/contrib/debian .

  # xenial need a g++8 to get the code compiled so we have a custom
  # control file for that, so that we can avoid to pollute the lsit
  # dependencies for the other supported ubuntu versions.
  # also for xenial we used autotools rather than cmake.
  if [ ${DISTRO} == "xenial" ]; then
    cp debian/xenial/* debian/
  fi

  # Generate the changelog for this package
  cat > debian/changelog <<EOF
${CONTROL_SOURCE_NAME} (${PACKAGE_VERSION}-${DISTRO}${DEBIAN_PKG_VERSION}) ${DISTRO}; urgency=medium

  * New upstream release.

  For an exhaustive list of changes please have a look at https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/blob/master/doc/release-notes.md

 -- ${SIGNER}  ${DATE}
EOF

  # if this is the fist try to upload we should als o need to upload
  # the orig.tar.gz containing the source code, hence we need to use "-sa"
  if [ $DEBIAN_PKG_VERSION -eq 1 ]; then
    debuild -S -sa
  else
    debuild -S -sd
  fi

  rm -rf debian
  popd

  if [ "${DRY_RUN}" == "false" ]; then
    dput "${PPA}" "${PACKAGE_NAME}_source.changes"
  else
    echo "Info: Dry run. Skipping upload to PPA for '${DISTRO}'."
  fi
}

if [ -z "${UBUNTU_NAME}" ]; then
  # Xenial: Ubuntu 16.04 LTS
  package "xenial"
  # Bionic: Ubuntu 18.04 LTS
  package "bionic"
  # Eoan: Ubuntu 19.10
  package "eoan"
  # Focal: Ubuntu 20.04 LTS
  package "focal"
else
  package ${UBUNTU_NAME}
fi

popd
