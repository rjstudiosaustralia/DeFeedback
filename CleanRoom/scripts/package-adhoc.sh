#!/bin/sh
set -eu

configuration="${1:-Release}"
build_dir="${2:-build}"
version="0.1.0"
artefacts="${build_dir}/CleanRoom/RingGuardPrototype_artefacts/${configuration}"
au="${artefacts}/AU/RingGuard Prototype.component"
vst3="${artefacts}/VST3/RingGuard Prototype.vst3"
standalone="${artefacts}/Standalone/RingGuard Prototype.app"
staging="dist/RingGuard Prototype ${version} arm64"
archive="dist/RingGuard-Prototype-${version}-adhoc-arm64.zip"
temporary_archive="dist/.RingGuard-Prototype-${version}-adhoc-arm64.tmp.zip"

for bundle in "${au}" "${vst3}" "${standalone}"; do
    if [ ! -d "${bundle}" ]; then
        echo "Missing ${bundle}; build the Release configuration first." >&2
        exit 1
    fi

done

mkdir -p dist
rm -rf "${staging}"
mkdir -p "${staging}/Audio Unit" "${staging}/VST3" "${staging}/Standalone"

# Re-sign final bundle contents because the manifest helper and final link may
# update files after JUCE initially signs them.
for bundle in "${au}" "${vst3}" "${standalone}"; do
    codesign --force --deep --sign - "${bundle}"
    codesign --verify --deep --strict --verbose=2 "${bundle}"
done

ditto "${au}" "${staging}/Audio Unit/RingGuard Prototype.component"
ditto "${vst3}" "${staging}/VST3/RingGuard Prototype.vst3"
ditto "${standalone}" "${staging}/Standalone/RingGuard Prototype.app"

cat > "${staging}/READ ME FIRST.txt" <<'EOF'
RingGuard Prototype 0.1.0 — private engineering build

This is a clean-room deterministic feedback-risk research prototype. It is not
Alpha Labs De-Feedback, does not yet include the planned learned vocal-isolation
or dereverberation model, and must not be relied on as the only protection for
loudspeakers, performers, audiences, or hearing.

The bundles are ad-hoc signed for private hardware qualification. They are not
notarized for public distribution.
EOF

rm -f "${temporary_archive}" "${archive}"
ditto -c -k --sequesterRsrc --keepParent "${staging}" "${temporary_archive}"
mv -f "${temporary_archive}" "${archive}"
rm -rf "${staging}"

shasum -a 256 "${archive}"
echo "Packaged: ${archive}"
