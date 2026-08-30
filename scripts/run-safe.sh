#!/bin/sh
set -eu

configuration="${1:-Debug}"
app="build-${configuration}/DeFeedbackLive_artefacts/${configuration}/DeFeedback Live.app"

if [ ! -d "${app}" ]; then
    echo "Missing ${app}; run ./scripts/build.sh ${configuration} first." >&2
    exit 1
fi

open -n "${app}" --args --safe
