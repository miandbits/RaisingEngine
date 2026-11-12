#!/bin/bash
set -euo pipefail
base=.cache/ios-vendor-ci
fixture=.github/diagnostics/ios-vendor-isolation
export_dir="$base/export"
engine_build="$export_dir/build-iphonesimulator"
products="$engine_build/native-player/Release-iphonesimulator"
sdl="$engine_build/native-player/vendor/SDL/Release-iphonesimulator/libSDL3.a"
apps="$base/apps"
engine_bundle="$apps/engine/build-iphonesimulator/native-player/Release-iphonesimulator"
mkdir -p "$engine_bundle" "$apps/U.app" "$base/transfer"

test "$(git -C native/player/vendor/SDL-ios rev-parse HEAD)" = 829a65d769d935c4852f8159e964312c0957260a
engine_hash_before="$(shasum -a 256 "$products/RaisingPlayer.app/RaisingPlayer")"
sdl_hash_before="$(shasum -a 256 "$sdl")"
cmake -S "$export_dir" -B "$engine_build" -DRAISING_VENDOR_CONTACT_REFERENCE=ON 2>&1 | tee "$base/reference-configure.log"
cmake --build "$engine_build" --config Release --target raising-sdl-contact-reference --parallel "$BUILD_JOBS" 2>&1 | tee "$base/reference-sdl-build.log"
test "$engine_hash_before" = "$(shasum -a 256 "$products/RaisingPlayer.app/RaisingPlayer")"
test "$sdl_hash_before" = "$(shasum -a 256 "$sdl")"
cp -R "$products/RaisingPlayer.app" "$engine_bundle/RaisingPlayer.app"
cp -R "$products/RaisingSDLContact.app" "$apps/S.app"

sdk="$(xcrun --sdk iphonesimulator --show-sdk-path)"
cp "$fixture/U-Info.plist" "$apps/U.app/Info.plist"
xcrun --sdk iphonesimulator swiftc -parse-as-library -module-name ContactReference \
  -target arm64-apple-ios16.0-simulator -sdk "$sdk" -framework UIKit \
  "$fixture/U-Reference.swift" -o "$apps/U.app/RaisingUIKitContact" 2>&1 | tee "$base/reference-uikit-build.log"
/usr/bin/codesign --force --sign - "$apps/U.app"

grep -E '^SDL_(STATIC|SHARED|TEST_LIBRARY|TESTS|EXAMPLES):BOOL=' "$engine_build/CMakeCache.txt" > "$base/sdl-build-options.txt"
{
  echo 'sdlCommit=829a65d769d935c4852f8159e964312c0957260a'
  echo "$sdl_hash_before"
  echo "$engine_hash_before"
  file "$sdl" "$apps/S.app/RaisingSDLContact" "$apps/U.app/RaisingUIKitContact"
} > "$base/producer-provenance.txt"
git ls-files -z -- .github/diagnostics/ios-vendor-ci .github/diagnostics/ios-vendor-isolation \
  .github/workflows/ios-simulator.yml native/player/CMakeLists.txt native/player/main.cpp native/player/diagnostics \
  native/player/simulator-input-proof.hpp native/mobile/ios config/native-dependencies.json tests/ios-system-ui \
  scripts/lib/ios-vendor-observation.mjs scripts/test-ios-vendor-reference.mjs scripts/test-ios-vendor-pair.mjs scripts/test-ios-system-ui.mjs \
  tests/mobile/ios-vendor-observation.test.mjs \
  | xargs -0 shasum -a 256 > "$base/transfer/sources.sha256"
git rev-parse HEAD > "$base/transfer/source.txt"
git rev-parse 'HEAD^{tree}' > "$base/transfer/tree.txt"
(
  cd "$base"
  shasum -a 256 apps/U.app/RaisingUIKitContact apps/S.app/RaisingSDLContact \
    apps/engine/build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app/RaisingPlayer \
    apps/engine/build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app/game/game.json \
    > transfer/apps.sha256
  tar -cf transfer/apps.tar apps
)
(
  cd "$base/transfer"
  shasum -a 256 source.txt tree.txt sources.sha256 apps.sha256 apps.tar > transfer.sha256
)
