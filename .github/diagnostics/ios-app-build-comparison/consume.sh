#!/usr/bin/env bash
set -euo pipefail
diag=.github/diagnostics/ios-app-build-comparison
work="$PWD/.cache/ios-app-build-comparison"
export_root="$work/consumer"
transfer="$work/transfer"
bundle="$export_root/build-iphonesimulator/native-player/Release-iphonesimulator"
app="$bundle/RaisingPlayer.app"
test ! -e "$export_root"
test ! -L "$export_root"
test "$(uname -m)" = arm64
test -x "$DEVELOPER_DIR/usr/bin/xcodebuild"
test "$(xcodebuild -version)" = "$(printf 'Xcode 26.6\nBuild version 17F113')"
test "$(xcrun --sdk iphonesimulator --show-sdk-version)" = 26.5
test "$(shasum -a 256 tests/ios-system-ui/SystemUITests.swift | awk '{print $1}')" = 01a0e6362e02eb1b922a75138305119d6a933fdcb44cca6dba88f9ac1510a2b2
for key in SDL_EVENT_LOGGING RAISING_INPUT_TRACE SIMCTL_CHILD_SDL_EVENT_LOGGING SIMCTL_CHILD_RAISING_INPUT_TRACE; do
  test -z "${!key+x}"
done
(cd "$transfer" && shasum -a 256 -c transfer.sha256)
test "$(cat "$transfer/source.txt")" = "$(git rev-parse HEAD)"
test "$(cat "$transfer/tree.txt")" = "$(git rev-parse 'HEAD^{tree}')"
test "$(cat "$transfer/arm.txt")" = "$BUILD_ARM"
test "$(cat "$transfer/prepared.sha")" = "$(jq -r .preparedArchiveSha256 "$work/verified-pair/pair.json")"
test "$(cat "$transfer/executable.sha")" = "$(jq -r --arg arm "$BUILD_ARM" '.[$arm].executableSha256' "$work/verified-pair/pair.json")"
test "$(cat "$transfer/game.sha")" = "$(jq -r .gameJsonSha256 "$work/verified-pair/pair.json")"
test "$(cat "$transfer/source.txt")" = "$(jq -r .source "$work/verified-pair/pair.json")"
test "$(cat "$transfer/tree.txt")" = "$(jq -r .tree "$work/verified-pair/pair.json")"
test "$(shasum -a 256 "$transfer/starter-app.tar" | awk '{print $1}')" = "$(jq -r --arg arm "$BUILD_ARM" '.[$arm].bundleArchiveSha256' "$work/verified-pair/pair.json")"
test "$(shasum -a 256 "$transfer/app-manifest.json" | awk '{print $1}')" = "$(jq -r --arg arm "$BUILD_ARM" '.[$arm].bundleManifestSha256' "$work/verified-pair/pair.json")"
python3 "$diag/files.py" check-archive "$transfer/starter-app.tar" RaisingPlayer.app
mkdir -p "$bundle"
tar -xf "$transfer/starter-app.tar" -C "$bundle"
python3 "$diag/files.py" bundle "$app" > "$work/consumer-app-manifest.json"
cmp "$transfer/app-manifest.json" "$work/consumer-app-manifest.json"
test "$(shasum -a 256 "$app/RaisingPlayer" | awk '{print $1}')" = "$(cat "$transfer/executable.sha")"
test "$(shasum -a 256 "$app/game/game.json" | awk '{print $1}')" = "$(cat "$transfer/game.sha")"
{
  echo "source=$(git rev-parse HEAD)"
  echo "tree=$(git rev-parse 'HEAD^{tree}')"
  echo "observation=$OBSERVATION"
  echo "buildArm=$BUILD_ARM"
  echo "runnerImage=${ImageOS:-unknown} ${ImageVersion:-unknown}"
  echo "testRuntime=$RAISING_IOS_SIMULATOR_RUNTIME"
  echo "testDeviceType=$RAISING_IOS_SIMULATOR_DEVICE_TYPE"
  uname -m
  sw_vers
  xcodebuild -version
  xcrun simctl list runtimes available
} | tee "$work/files-environment.txt"
grep -F '23F77' "$work/files-environment.txt" | grep -F "$RAISING_IOS_SIMULATOR_RUNTIME"
