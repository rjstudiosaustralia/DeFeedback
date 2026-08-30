#!/bin/sh
set -eu

configuration="${1:-Release}"
build_dir="${2:-build-${configuration}}"
app="${build_dir}/DeFeedbackLive_artefacts/${configuration}/DeFeedback Live.app"
archive="dist/DeFeedback-Live-0.3.0-adhoc-arm64.zip"
temporary_archive="dist/.DeFeedback-Live-0.3.0-adhoc-arm64.tmp.zip"

if [ ! -d "${app}" ]; then
    echo "Missing ${app}; build the Release configuration first." >&2
    exit 1
fi

mkdir -p dist

# CMake may update the executable after JUCE created the bundle signature.
# Re-sign the completed tree before packaging so the delivered app verifies.
codesign --force --deep --sign - "${app}"
codesign --verify --deep --strict --verbose=2 "${app}"

rm -f "${temporary_archive}"
ditto -c -k --keepParent "${app}" "${temporary_archive}"
mv -f "${temporary_archive}" "${archive}"

shasum -a 256 "${archive}"
echo "Packaged: ${archive}"
