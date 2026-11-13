#!/usr/bin/env bash
set -euo pipefail
diag=.github/diagnostics/ios-app-build-comparison
work=.cache/ios-app-build-comparison
src="$work/prepared"
out="$work/common"
test ! -e "$src"
test ! -L "$src"
mkdir -p "$out"
node "$diag/export.mjs"
test -f "$src/build-iphonesimulator/CMakeCache.txt"
test -f "$src/xcode-configure.log"
mv "$src/build-iphonesimulator" "$work/export-only-build"
mv "$src/xcode-configure.log" "$work/export-configure.log"
test ! -e "$src/xcode-build.log"
cp "$src/ios-build.json" "$out/ios-build-original.json"
python3 "$diag/files.py" prepared "$src" > "$out/input-manifest.json"
git rev-parse HEAD > "$out/source.txt"
git rev-parse 'HEAD^{tree}' > "$out/tree.txt"
tar -cf "$out/prepared.tar" -C "$work" prepared
python3 "$diag/files.py" check-archive "$out/prepared.tar" prepared
(cd "$out" && shasum -a 256 source.txt tree.txt ios-build-original.json input-manifest.json prepared.tar > transfer.sha256)
