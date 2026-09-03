#!/bin/sh
set -eu

configuration="${1:-Release}"
build_dir="${2:-build-${configuration}}"
app="${build_dir}/DeFeedbackLive_artefacts/${configuration}/DeFeedback Live.app"

if [ ! -d "${app}" ]; then
    echo "Missing ${app}; build the Release configuration first." >&2
    exit 1
fi

binary="${app}/Contents/MacOS/DeFeedback Live"
minimum_macos=$(/usr/bin/vtool -show-build "${binary}" | awk '/minos/{print $2; exit}')
architectures=$(/usr/bin/lipo -archs "${binary}")

if [ "${minimum_macos}" != "13.0" ]; then
    echo "Refusing to package: binary minimum macOS is ${minimum_macos:-unknown}, expected 13.0." >&2
    exit 1
fi

if [ "${architectures}" != "arm64" ]; then
    echo "Refusing to package: binary architectures are ${architectures:-unknown}, expected arm64." >&2
    exit 1
fi

version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "${app}/Contents/Info.plist")
archive="dist/DeFeedback-Live-${version}-adhoc-arm64.zip"
temporary_archive="dist/.DeFeedback-Live-${version}-adhoc-arm64.tmp.zip"

mkdir -p dist

# Public packages carry the licence, safety notice, and corresponding-source
# location inside the application bundle.
cp LICENSE "${app}/Contents/Resources/LICENSE"
cp DISCLAIMER.md "${app}/Contents/Resources/DISCLAIMER.md"
cp SOURCE.md "${app}/Contents/Resources/SOURCE.md"

# CMake may update the executable after JUCE created the bundle signature.
# Re-sign the completed tree before packaging so the delivered app verifies.
codesign --force --deep --sign - "${app}"
codesign --verify --deep --strict --verbose=2 "${app}"

rm -f "${temporary_archive}"
ditto -c -k --norsrc --keepParent "${app}" "${temporary_archive}"
mv -f "${temporary_archive}" "${archive}"

shasum -a 256 "${archive}"
echo "Minimum macOS: ${minimum_macos}"
echo "Architecture: ${architectures}"
echo "Packaged: ${archive}"
