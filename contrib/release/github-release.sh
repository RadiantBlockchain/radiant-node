#!/usr/bin/env bash

export LC_ALL=C

set -euo pipefail

SCRIPT_PATH="$(dirname "$0")"
ORIGINAL_PWD="$(pwd)"
TOPLEVEL="$(cd "${SCRIPT_PATH}"; git rev-parse --show-toplevel)"
OAUTH_TOKEN_PATH="${PWD}/.github-oauth-token"
RELEASE_NOTES_DIR="${TOPLEVEL}/doc/release-notes"

help_message() {
  echo "Create a draft Github release and upload binaries."
  echo "Usage: $0 <options>"
  echo "-a, --asset-dir      (required) Path to the top-level directory outputted by a Gitian build."
  echo "                        This directory must contain linux, osx, and win binaries in those respective sub-directories."
  echo "-d, --dry-run         Run through the script, but do not touch existing tags, push to Github, or upload release files."
  echo "-h, --help            Display this help message."
  echo "-o, --oauth-token     Path to a file containing your OAuth token (defaults to: '${OAUTH_TOKEN_PATH}')."
  echo "-r, --release-notes   Path to the release notes file (defaults to: '${RELEASE_NOTES_DIR}/release-notes-<version>.md')."
  echo "-t, --tag             (required) The git tag create a release for. This tag must already exist."
}

ASSET_DIR=""
DRY_RUN="false"
RELEASE_NOTES_PATH=""
TAG=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
case "$1" in
  -a|--assets-dir)
    ASSET_DIR=$(cd "$2"; pwd)
    shift # shift past argument
    shift # shift past value
    ;;
  -d|--dry-run)
    DRY_RUN="true"
    shift # shift past argument
    ;;
  -h|--help)
    help_message
    exit 0
    ;;
  -o|--oauth-token)
    OAUTH_TOKEN_PATH="$2"
    shift # shift past argument
    shift # shift past value
    ;;
  -r|--release-notes)
    RELEASE_NOTES_PATH="$2"
    shift # shift past argument
    shift # shift past value
    ;;
  -t|--tag)
    TAG="$2"
    shift # shift past argument
    shift # shift past value
    ;;
  *)
    echo "Unknown argument: $1"
    help_message
    exit 1
    ;;
esac
done

# jq must be installed
if ! command -v jq > /dev/null; then
  echo "Error: 'jq' is not installed."
  exit 10
fi

# Sanity checks on the release tag
if [ -z "${TAG}" ]; then
  echo "Error: The release tag was not set. Try setting it with [ -t | --tag ]"
  exit 11
fi
TAG_PATTERN="^v[0-9]*\.[0-9]*\.[0-9]*"
if [[ ! ${TAG} =~ ${TAG_PATTERN} ]]; then
  echo "Error: Tag '${TAG}' does not match the expected versioning pattern '${TAG_PATTERN}'"
  exit 12
fi

# Fetch OAuth token
if [ ! -f "${OAUTH_TOKEN_PATH}" ]; then
  echo "Error: OAuth token file '${OAUTH_TOKEN_PATH}' does not exist"
  exit 13
fi
OAUTH_TOKEN=$(cat "${OAUTH_TOKEN_PATH}")
if [ -z "${OAUTH_TOKEN}" ]; then
  echo "Error: OAuth token is empty"
  exit 14
fi

# Fetch remote tags and make sure the tag exists
cd "${SCRIPT_PATH}"
GIT_REPO="https://${OAUTH_TOKEN}@github.com/radiant-node/radiant-node.git"
git fetch "${GIT_REPO}" tag "${TAG}" || (echo "Error: Remote does not have tag '${TAG}'." && exit 20)
cd "${ORIGINAL_PWD}"

VERSION=$(cut -c 2- <<< "${TAG}")

# Collect list of assets (binaries) to upload
if [ -z "${ASSET_DIR}" ]; then
  echo "Error: Asset directory was not set. Try setting it with [ -a | --asset-dir ]"
  exit 30
fi
ASSET_LIST=()
if [ -d "${ASSET_DIR}" ]; then
  # Linux binaries
  ASSET_LIST+=("${ASSET_DIR}/linux/radiant-node-${VERSION}-aarch64-linux-gnu.tar.gz")
  ASSET_LIST+=("${ASSET_DIR}/linux/radiant-node-${VERSION}-arm-linux-gnueabihf.tar.gz")
  ASSET_LIST+=("${ASSET_DIR}/linux/radiant-node-${VERSION}-i686-pc-linux-gnu.tar.gz")
  ASSET_LIST+=("${ASSET_DIR}/linux/radiant-node-${VERSION}-x86_64-linux-gnu.tar.gz")

  # OSX binaries
  ASSET_LIST+=("${ASSET_DIR}/osx/radiant-node-${VERSION}-osx-unsigned.dmg")

  # Windows binaries
  ASSET_LIST+=("${ASSET_DIR}/win/radiant-node-${VERSION}-win32-setup-unsigned.exe")
  ASSET_LIST+=("${ASSET_DIR}/win/radiant-node-${VERSION}-win64-setup-unsigned.exe")

  for FILENAME in "${ASSET_LIST[@]}"; do
    if [ ! -f "${FILENAME}" ]; then
      echo "Error: Expected binary '${FILENAME}' does not exist"
      exit 31
    fi
  done

  # Add any signature files
  for FILENAME in "${ASSET_DIR}"/*"-sha256sums.${VERSION}.asc"; do
    ASSET_LIST+=("${FILENAME}")
  done
else
  echo "Error: Asset directory '${ASSET_DIR}' does not exist"
  exit 32
fi

# Fetch release notes
if [ -z "${RELEASE_NOTES_PATH}" ]; then
  RELEASE_NOTES_PATH="${RELEASE_NOTES_DIR}/release-notes-${VERSION}.md"
fi
RELEASE_NOTES=$(jq -Rs '.' "${RELEASE_NOTES_PATH}")
if [ "${RELEASE_NOTES}" == '""' ]; then
  echo "Error: Could not fetch release notes for version '${VERSION}'"
  exit 40
fi

# Format request data
POST_DATA="{\"tag_name\": \"${TAG}\", \"name\": \"${VERSION}\", \"body\": ${RELEASE_NOTES}, \"draft\": true}"
URL="https://api.github.com/repos/radiant-node/radiant-node/releases"

if [ "${DRY_RUN}" == "true" ]; then
  echo "POST request data that would be sent to '${URL}':"
  printf '%s\n\n' "${POST_DATA}"
  echo "Would attempt to upload these files:"
  for FILENAME in "${ASSET_LIST[@]}"; do
    echo "${FILENAME}"
  done
else
  echo "Creating draft release..."
  echo "Requesting '${URL}'..."
  RESPONSE=$(curl -X POST -H "Content-Type: application/json" -H "Authorization: token ${OAUTH_TOKEN}" -d "${POST_DATA}" "${URL}")
  RELEASE_ID=$(jq '.id' <<< "${RESPONSE}")
  UPLOAD_URL="https://uploads.github.com/repos/radiant-node/radiant-node/releases/${RELEASE_ID}/assets"

  echo "Uploading assets..."
  if [ ${#ASSET_LIST[@]} -gt 0 ]; then
    for FILEPATH in "${ASSET_LIST[@]}"; do
      echo "Uploading '${FILEPATH}'..."
      FILENAME=$(basename "${FILEPATH}")
      curl -X POST -H "Content-Type:application/octet-stream" -H "Authorization: token ${OAUTH_TOKEN}" --data-binary "@${FILEPATH}" "${UPLOAD_URL}?name=${FILENAME}"
    done
  fi
  echo
  echo "Done uploading assets."
fi

echo "Done."
echo "https://github.com/radiant-node/radiant-node/releases/tag/${TAG}"
