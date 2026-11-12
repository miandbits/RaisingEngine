#!/bin/bash
set -euo pipefail
test -x "$DEVELOPER_DIR/usr/bin/xcodebuild"
mkdir -p .cache/ios-vendor-ci
{
  echo "source=$(git rev-parse HEAD)"
  echo "tree=$(git rev-parse 'HEAD^{tree}')"
  echo "runnerImage=${ImageOS:-unknown}/${ImageVersion:-unknown}"
  echo "testRuntime=$RAISING_IOS_SIMULATOR_RUNTIME"
  echo "testDeviceType=$RAISING_IOS_SIMULATOR_DEVICE_TYPE"
  xcodebuild -version
  xcodebuild -version -sdk iphonesimulator
  xcrun simctl list runtimes available
} | tee .cache/ios-vendor-ci/files-environment.txt
grep -Fx 'Xcode 26.6' .cache/ios-vendor-ci/files-environment.txt
grep -Fx 'Build version 17F113' .cache/ios-vendor-ci/files-environment.txt
grep -F '23F77) - com.apple.CoreSimulator.SimRuntime.iOS-26-5' .cache/ios-vendor-ci/files-environment.txt
