#!/bin/sh
set -eu

configuration="${1:-Release}"
build_dir="build-${configuration}"

cmake -S . -B "${build_dir}" -DCMAKE_BUILD_TYPE="${configuration}"
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

echo "Built: ${build_dir}/DeFeedbackLive_artefacts/${configuration}/DeFeedback Live.app"
