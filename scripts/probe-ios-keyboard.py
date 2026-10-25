#!/usr/bin/env python3
"""Observe Korean composition in plain UIKit, without an engine or game."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mode', choices=['typing', 'mid-syllable-rotation'], default='typing')
parser.add_argument('--family', choices=['ipad', 'iphone'], default='ipad')
parser.add_argument('--runtime', help='Exact installed simulator runtime identifier; defaults to the last matching available runtime.')
args = parser.parse_args()
if sys.platform != 'darwin':
    parser.error('This probe requires macOS, Xcode and an installed iOS Simulator runtime.')
root = Path(__file__).resolve().parent.parent
cache = root / '.cache/ios-keyboard-reference'
cache.mkdir(parents=True, exist_ok=True)
out = Path(tempfile.mkdtemp(prefix='run-', dir=cache))
app = out / 'UIKitReference.app'
app.mkdir()
app_id = 'org.raisingengine.keyboardreference.' + out.name.replace('-', '')
source = root / 'tests/ios-system-ui/UIKitReference.swift'
harness = out / 'harness'
shutil.copytree(root / 'tests/ios-system-ui', harness)
test_source = (harness / 'KeyboardReferenceTests.swift').read_text()
test_source = test_source.replace('__APP_ID__', app_id).replace('__PROBE_MODE__', args.mode)
(harness / 'SystemUITests.swift').write_text(test_source)
plist = {
    'CFBundleExecutable': 'UIKitReference', 'CFBundleIdentifier': app_id,
    'CFBundleName': 'UIKit Reference', 'CFBundleDisplayName': 'UIKit Reference',
    'CFBundlePackageType': 'APPL', 'CFBundleVersion': '1',
    'CFBundleShortVersionString': '1.0', 'MinimumOSVersion': '16.0',
    'UIDeviceFamily': [1, 2], 'UILaunchScreen': {},
    'UISupportedInterfaceOrientations': ['UIInterfaceOrientationPortrait',
        'UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight'],
    'UIApplicationSceneManifest': {'UIApplicationSupportsMultipleScenes': False,
        'UISceneConfigurations': {'UIWindowSceneSessionRoleApplication': [{
            'UISceneConfigurationName': 'Default',
            'UISceneDelegateClassName': 'UIKitReference.SceneDelegate'}]}}
}
(app / 'Info.plist').write_bytes(plistlib.dumps(plist))
receipt = {
    'kind': 'standalone-uikit-keyboard-reference', 'mode': args.mode,
    'driverSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    'family': args.family, 'completed': False, 'textMatched': None,
    'productAcceptance': False, 'appId': app_id, 'out': str(out),
    'limits': ['Simulator and XCTest input only; no physical-device acceptance.',
               'A successful observation collection is not a passing composition result.',
               'Standard UIKit text field with no engine, game, or text normalization.']
}
device = None

def tool(command, log, timeout=180):
    with (out / log).open('w') as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f'{log}: exit {result.returncode}')

def sim(*values):
    return subprocess.check_output(['/usr/bin/xcrun', 'simctl', *values], text=True, timeout=30).strip()

try:
    sdk = subprocess.check_output(['/usr/bin/xcrun', '--sdk', 'iphonesimulator', '--show-sdk-path'], text=True).strip()
    arch = subprocess.check_output(['/usr/bin/uname', '-m'], text=True).strip()
    tool(['/usr/bin/xcrun', '--sdk', 'iphonesimulator', 'swiftc', '-parse-as-library',
          '-module-name', 'UIKitReference', '-target', arch + '-apple-ios16.0-simulator',
          '-sdk', sdk, '-module-cache-path', str(out / 'module-cache'), '-framework', 'UIKit',
          str(source), '-o', str(app / 'UIKitReference')], 'app-build.log')
    tool(['/usr/bin/codesign', '--force', '--sign', '-', str(app)], 'codesign.log')
    inventory = json.loads(sim('list', 'devices', 'available', '--json'))['devices']
    candidates = [(runtime, item['deviceTypeIdentifier'])
                  for runtime, devices in inventory.items() if '.iOS-' in runtime and (args.runtime is None or runtime == args.runtime)
                  for item in devices if args.family in item['name'].lower()]
    if not candidates:
        raise RuntimeError('Install a matching iOS Simulator runtime first.')
    runtime, kind = candidates[-1]
    device = sim('create', 'RaisingEngine UIKit reference ' + out.name, kind, runtime)
    receipt['device'] = {'id': device, 'type': kind, 'runtime': runtime}
    (out / 'result.json').write_text(json.dumps(receipt, indent=2))
    sim('boot', device)
    tool(['/usr/bin/xcrun', 'simctl', 'bootstatus', device, '-b'], 'boot.log')
    sim('install', device, str(app))
    build = ['-project', str(harness / 'SystemUI.xcodeproj'), '-scheme', 'SystemUITests',
             '-destination', 'platform=iOS Simulator,id=' + device,
             '-derivedDataPath', str(out / 'DerivedData'), 'CODE_SIGNING_ALLOWED=NO']
    tool(['/usr/bin/xcodebuild', 'build-for-testing', *build], 'test-build.log')
    sim('launch', '--stdout=' + str(out / 'app.stdout'), '--stderr=' + str(out / 'app.stderr'), device, app_id)
    tool(['/usr/bin/xcodebuild', 'test-without-building', *build,
          '-parallel-testing-enabled', 'NO', '-resultBundlePath', str(out / 'reference.xcresult'),
          '-only-testing:SystemUITests/SystemUITests/testKoreanTyping'], 'test.log')
    matches = re.findall(r'REFERENCE_OBSERVATION (\{.*\})', (out / 'test.log').read_text())
    if not matches:
        raise RuntimeError('The reference test did not record an observation.')
    receipt['observation'] = json.loads(matches[-1])
    receipt['completed'] = True
    receipt['textMatched'] = receipt['observation']['compositionPreserved']
except Exception as error:
    receipt['error'] = str(error)
finally:
    receipt['finishedAt'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    receipt['hashes'] = {name: hashlib.sha256(file.read_bytes()).hexdigest()
        for name, file in [('Reference.swift', source), ('SystemUITests.swift', harness / 'SystemUITests.swift'),
                           ('UIKitReference', app / 'UIKitReference'), ('Info.plist', app / 'Info.plist')]
        if file.is_file()}
    if device:
        for command in [('shutdown', device), ('delete', device)]:
            try:
                sim(*command)
            except Exception as error:
                receipt.setdefault('cleanupWarnings', []).append(str(error))
    (out / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))
raise SystemExit(0 if receipt['completed'] and receipt['textMatched'] else 1)
