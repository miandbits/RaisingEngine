#!/bin/bash
set -euo pipefail
test "${GITHUB_ACTIONS:-}" = true
test "${RUNNER_ENVIRONMENT:-}" = github-hosted
test "${RUNNER_OS:-}" = macOS
test "${RUNNER_ARCH:-}" = ARM64
test "${GITHUB_REF:-}" = refs/heads/codex/ios-idb-route-comparison
umask 077

base="$RUNNER_TEMP/raising-idb-1.7.8"
mkdir "$base"
archive="$base/idb-companion.macos-arm64.tar.gz"
curl --fail --silent --show-error --location --connect-timeout 15 --max-time 120 \
  --output "$archive" https://github.com/meta/idb/releases/download/v1.7.8/idb-companion.macos-arm64.tar.gz
echo "44369a2df0b62e3a1764de61bd09280999c7e9b27ea8f20bff1ea3a9db0302a2  $archive" | shasum -a 256 -c
mkdir "$base/companion"
tar -xf "$archive" -C "$base/companion"
test -x "$base/companion/idb_companion"
echo "1178785532e66acdaa1944a1a8898796af3b449bcd7bf800e186b246d56eb1f3  $base/companion/idb_companion" | shasum -a 256 -c
python -m venv "$base/python"
"$base/python/bin/python" -m pip --isolated --disable-pip-version-check install \
  --require-hashes --only-binary=:all: --no-deps --index-url https://pypi.org/simple \
  --requirement .github/diagnostics/ios-idb/requirements.txt
"$base/python/bin/python" -m pip check
"$base/python/bin/python" -c 'from idb.grpc.idb_grpc import CompanionServiceStub; from idb.grpc.idb_pb2 import HIDEvent'
{
  echo "RAISING_IDB_COMPANION=$base/companion/idb_companion"
  echo "RAISING_IDB_PYTHON=$base/python/bin/python"
} >> "$GITHUB_ENV"
mkdir -p .cache/ios-simulator-ci
{
  echo 'idbSource=c793afad2a31092628f7013cf1aa96f266c85024'
  shasum -a 256 "$archive" "$base/companion/idb_companion" .github/diagnostics/ios-idb/requirements.txt
  file "$base/companion/idb_companion"
  "$base/python/bin/python" --version
  "$base/python/bin/python" -m pip list --format freeze
} > .cache/ios-simulator-ci/idb-dependencies.txt
