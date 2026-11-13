#!/usr/bin/env bash
set -euo pipefail
diag=.github/diagnostics/ios-app-build-comparison
work="$PWD/.cache/ios-app-build-comparison"
input="$work/common"
src="$work/prepared"
out="$work/built"
scene="$src/engine/native/player/vendor/SDL-ios/src/video/uikit/SDL_uikitvideo.m"
app="$src/build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app"
mkdir -p "$out"
test ! -e "$src"
test ! -L "$src"
(cd "$input" && shasum -a 256 -c transfer.sha256)
test "$(cat "$input/source.txt")" = "$(git rev-parse HEAD)"
test "$(cat "$input/tree.txt")" = "$(git rev-parse 'HEAD^{tree}')"
python3 "$diag/files.py" check-archive "$input/prepared.tar" prepared
tar -xf "$input/prepared.tar" -C "$work"
test ! -e "$src/build-iphonesimulator"
test ! -e "$src/xcode-configure.log"
test ! -e "$src/xcode-build.log"
cmp "$src/ios-build.json" "$input/ios-build-original.json"
python3 "$diag/files.py" prepared "$src" > "$out/input-before.json"
cmp "$input/input-manifest.json" "$out/input-before.json"
test "$(uname -m)" = arm64
test -x "$DEVELOPER_DIR/usr/bin/xcodebuild"
sdk_path=$(xcrun --sdk iphonesimulator --show-sdk-path)
sdk_version=$(xcrun --sdk iphonesimulator --show-sdk-version)
test "$sdk_version" = "$EXPECTED_SDK"
test "$sdk_path" = "$DEVELOPER_DIR/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator$EXPECTED_SDK.sdk"
toolchain=$(xcodebuild -version)
test "$toolchain" = "$(printf 'Xcode %s\nBuild version %s' "$EXPECTED_XCODE" "$EXPECTED_XCODE_BUILD")"
{
  echo "source=$(git rev-parse HEAD)"
  echo "tree=$(git rev-parse 'HEAD^{tree}')"
  echo "arm=$BUILD_ARM"
  echo "runnerImage=${ImageOS:-unknown} ${ImageVersion:-unknown}"
  echo "developerDir=$DEVELOPER_DIR"
  uname -m
  sw_vers
  printf '%s\n' "$toolchain"
  echo "sdk=$sdk_path"
  echo "sdkVersion=$sdk_version"
  xcrun --sdk iphonesimulator --show-sdk-build-version
  xcrun --sdk iphonesimulator --find clang
  xcrun --sdk iphonesimulator clang --version
  xcrun --sdk iphonesimulator --find ld
  xcrun --sdk iphonesimulator ld -v 2>&1
  cmake --version
} | tee "$out/environment.txt"
npm run build:ios -- --project "$src"
python3 "$diag/files.py" prepared "$src" > "$out/input-after.json"
cmp "$out/input-before.json" "$out/input-after.json"
test "$(jq -S 'del(.status,.sdk,.signed,.verifiedMetadata)' "$src/ios-build.json")" = \
  "$(jq -S 'del(.status,.sdk,.signed,.verifiedMetadata)' "$input/ios-build-original.json")"
test "$(jq -r .status "$src/ios-build.json")" = simulator-built
toolchain_bin="$(dirname "$(xcrun --sdk iphonesimulator --find clang)")"
test "$toolchain_bin" = "$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin"
python3 "$diag/verify-compile.py" "$src/xcode-build.log" "$scene" "$app/RaisingPlayer" "$toolchain_bin" "$sdk_path" "$out"
grep -E '^CMAKE_(C|CXX|OBJC)_COMPILER(:|_)|^CMAKE_OSX_(SYSROOT|DEPLOYMENT_TARGET|ARCHITECTURES):|^CMAKE_HOME_DIRECTORY:' \
  "$src/build-iphonesimulator/CMakeCache.txt" > "$out/cmake-toolchain.txt"
shasum -a 256 "$scene" "$app/RaisingPlayer" > "$out/sdl-view-frame-sources.sha256"
# The regression reads pristine files from Git to construct its negative fixture.
# This separate checkout occurs after the app build and is outside its input tree.
fixture_repo=native/player/vendor/SDL-ios
test ! -e "$fixture_repo"
test ! -L "$fixture_repo"
read -r fixture_url fixture_ref fixture_commit < <(jq -r '.iosDependencies[] | select(.path == "SDL-ios") | [.url,.ref,.commit] | @tsv' config/native-dependencies.json)
git clone --depth 1 --branch "$fixture_ref" --single-branch "$fixture_url" "$fixture_repo"
test "$(git -C "$fixture_repo" rev-parse HEAD)" = "$fixture_commit"
RAISING_IOS_BUILT_SDL_SCENE_SOURCE="$scene" node --test tests/mobile/ios-keyboard-patch.test.mjs | tee "$out/sdl-view-frame-test.log"
plutil -convert json -o "$out/bundle-info.json" "$app/Info.plist"
xcrun vtool -show-build "$app/RaisingPlayer" > "$out/macho-build.txt"
test "$(lipo -archs "$app/RaisingPlayer")" = arm64
grep -Eq '^[[:space:]]*platform IOSSIMULATOR$' "$out/macho-build.txt"
grep -Eq '^[[:space:]]*minos 16\.0$' "$out/macho-build.txt"
test "$(awk '$1 == "sdk" {print $2}' "$out/macho-build.txt")" = "$EXPECTED_SDK"
test "$(jq -r .MinimumOSVersion "$out/bundle-info.json")" = 16.0
test "$(jq -r .DTSDKName "$out/bundle-info.json")" = "iphonesimulator$EXPECTED_SDK"
test "$(jq -r .DTXcodeBuild "$out/bundle-info.json")" = "$EXPECTED_XCODE_BUILD"
test "$(jq -r .CFBundleIdentifier "$out/bundle-info.json")" = "$(jq -r .applicationId "$input/ios-build-original.json")"
python3 "$diag/files.py" bundle "$app" > "$out/app-manifest.json"
shasum -a 256 "$app/RaisingPlayer" | awk '{print $1}' > "$out/executable.sha"
shasum -a 256 "$app/game/game.json" | awk '{print $1}' > "$out/game.sha"
test "$(cat "$out/game.sha")" = "$(shasum -a 256 "$src/game/game.json" | awk '{print $1}')"
shasum -a 256 "$input/prepared.tar" | awk '{print $1}' > "$out/prepared.sha"
cp "$input/source.txt" "$input/tree.txt" "$input/ios-build-original.json" "$out/"
printf '%s\n' "$BUILD_ARM" > "$out/arm.txt"
cp "$src/ios-build.json" "$out/ios-build-built.json"
tar -cf "$out/starter-app.tar" -C "$(dirname "$app")" RaisingPlayer.app
python3 "$diag/files.py" check-archive "$out/starter-app.tar" RaisingPlayer.app
(cd "$out" && shasum -a 256 arm.txt source.txt tree.txt prepared.sha input-before.json input-after.json ios-build-original.json ios-build-built.json bundle-info.json app-manifest.json executable.sha game.sha starter-app.tar > transfer.sha256)
