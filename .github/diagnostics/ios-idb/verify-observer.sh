#!/bin/bash
set -euo pipefail
transfer=.cache/ios-simulator-ci/transfer
bundle=.cache/ios-simulator-ci/export/build-iphonesimulator/native-player/Release-iphonesimulator
producer=043b0227f3a7ca9c3f070356f1a152323cfaa5a4
(cd "$transfer" && shasum -a 256 -c transfer.sha256)
test "$producer" = "$(cat "$transfer/source.txt")"
echo "413213214b7f118df7f2c3bf9f95de6381c62598e9617b5686e6811022361b81  $transfer/starter-app.tar" | shasum -a 256 -c
git fetch --no-tags --depth=1 origin "$producer"
test "$(git rev-parse "$producer^{tree}")" = ceea52bf25c34f579b3a540ff826c529fdf251ce
git diff --name-status "$producer" HEAD | tee .cache/ios-simulator-ci/source-diff.txt
git diff --exit-code "$producer" HEAD -- . \
  ':(exclude).github/workflows/ios-simulator.yml' \
  ':(exclude).github/workflows/ios-idb-diagnostic-pair.yml' \
  ':(exclude).github/diagnostics/ios-idb/**' \
  ':(exclude)scripts/lib/ios-host-contact-test.mjs' \
  ':(exclude)scripts/lib/ios-simulator-host-contact.swift' \
  ':(exclude)scripts/lib/ios-idb-contact-test.mjs' \
  ':(exclude)scripts/lib/ios-idb-contact.py' \
  ':(exclude)scripts/lib/ios-idb-contact.test.py' \
  ':(exclude)scripts/lib/ios-simulator-test.mjs' \
  ':(exclude)scripts/test-ios-idb-preflight.mjs' \
  ':(exclude)scripts/test-ios-system-ui.mjs' \
  ':(exclude)tests/ios-system-ui/SystemUITests.swift' \
  ':(exclude)tests/mobile/ios-host-contact-test.test.mjs' \
  ':(exclude)tests/mobile/ios-idb-contact-test.test.mjs' \
  ':(exclude)tests/visual/schedule-details.test.mjs'
tar -tvf "$transfer/starter-app.tar" > .cache/ios-simulator-ci/observer-members.txt
test "$(wc -l < .cache/ios-simulator-ci/observer-members.txt | tr -d ' ')" = 31
mkdir -p "$bundle"
tar -xf "$transfer/starter-app.tar" -C "$bundle"
shasum -a 256 -c "$transfer/bundle.sha256"
echo "dca7c40216dcaff7cb21cb2fb79bdb53b129b26dadcdafe3dea1fd990903941a  $bundle/RaisingPlayer.app/RaisingPlayer" | shasum -a 256 -c
echo "586d25e53bdfa9e99b373c03518bccc67ff3e3eed028011ba29a500594a73fae  $bundle/RaisingPlayer.app/game/game.json" | shasum -a 256 -c
shasum -a 256 native/player/main.cpp native/player/simulator-input-proof.hpp \
  tests/ios-system-ui/SystemUITests.swift scripts/test-ios-system-ui.mjs tests/mobile/ios-idb-contact-test.test.mjs \
  scripts/test-ios-idb-preflight.mjs scripts/lib/ios-simulator-test.mjs scripts/lib/ios-idb-contact* \
  .github/diagnostics/ios-idb/* .github/workflows/ios-{simulator,idb-diagnostic-pair}.yml \
  > .cache/ios-simulator-ci/sources.sha256
