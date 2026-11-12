#!/usr/bin/env python3
"""One ordinary UIKit button / one public XCTest coordinate press; diagnostic only."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out', required=True, help='Parent evidence directory; creates a unique run directory.')
parser.add_argument('--runtime', default='com.apple.CoreSimulator.SimRuntime.iOS-26-5')
parser.add_argument('--device-type', default='com.apple.CoreSimulator.SimDeviceType.iPhone-17e')
args = parser.parse_args()
if sys.platform != 'darwin':
    parser.error('Requires macOS, Xcode and an installed iOS Simulator runtime.')
source = Path(__file__).resolve().parent
parent = Path(args.out).resolve()
parent.mkdir(parents=True, exist_ok=True)
out = Path(tempfile.mkdtemp(prefix='run-', dir=parent))
app = out / 'ContactReference.app'
app.mkdir()
app_id = 'org.raisingengine.contactreference.' + out.name.replace('-', '').lower()
receipt = {'kind': 'standalone-uikit-single-contact', 'productAcceptance': False,
           'completed': False, 'contactAccepted': False, 'xctestPassed': False, 'appId': app_id, 'out': str(out),
           'requestedContact': {'x': 191, 'y': 572.5000305, 'durationSeconds': 0.20, 'count': 1},
           'tools': [], 'limits': ['One Simulator observation, not reliability or physical-device acceptance.',
           'No engine or game. A successful contact does not disprove intermittent upstream cancellation.']}
device = None

def save():
    (out / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n')

def tool(command, name, timeout=180, env=None):
    record = {'log': name, 'startedAt': datetime.datetime.now(datetime.timezone.utc).isoformat()}
    receipt['tools'].append(record)
    start = time.monotonic()
    try:
        with (out / name).open('w') as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout, env=env)
        record['exitCode'] = result.returncode
        if result.returncode:
            raise RuntimeError(f'{name}: exit {result.returncode}')
        return (out / name).read_text()
    except subprocess.TimeoutExpired:
        record['timedOut'] = True
        raise RuntimeError(f'{name}: timeout after {timeout}s')
    finally:
        record['elapsedSeconds'] = time.monotonic() - start
        save()

def sim(*args):
    return ['/usr/bin/xcrun', 'simctl', *args]

try:
    (out / 'fixture-source').mkdir()
    for name in ['ContactReference.swift', 'ContactReferenceTests.swift', 'run.py']:
        shutil.copy2(source / name, out / 'fixture-source' / name)
    receipt['sourceHashes'] = {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                              for name in ['ContactReference.swift', 'ContactReferenceTests.swift', 'run.py']}
    plist = {'CFBundleExecutable': 'ContactReference', 'CFBundleIdentifier': app_id,
             'CFBundleName': 'Contact Reference', 'CFBundleDisplayName': 'Contact Reference',
             'CFBundlePackageType': 'APPL', 'CFBundleVersion': '1', 'CFBundleShortVersionString': '1.0',
             'MinimumOSVersion': '16.0', 'UIDeviceFamily': [1], 'UILaunchScreen': {},
             'UISupportedInterfaceOrientations': ['UIInterfaceOrientationPortrait'],
             'UIApplicationSceneManifest': {'UIApplicationSupportsMultipleScenes': False,
             'UISceneConfigurations': {'UIWindowSceneSessionRoleApplication': [{
             'UISceneConfigurationName': 'Default', 'UISceneDelegateClassName': 'ContactReference.SceneDelegate'}]}}}
    (app / 'Info.plist').write_bytes(plistlib.dumps(plist))
    sdk = tool(['/usr/bin/xcrun', '--sdk', 'iphonesimulator', '--show-sdk-path'], 'sdk.log', 30).strip()
    arch = os.uname().machine
    tool(['/usr/bin/xcrun', '--sdk', 'iphonesimulator', 'swiftc', '-parse-as-library', '-module-name', 'ContactReference',
          '-target', arch + '-apple-ios16.0-simulator', '-sdk', sdk, '-module-cache-path', str(out / 'module-cache'),
          '-framework', 'UIKit', str(source / 'ContactReference.swift'), '-o', str(app / 'ContactReference')], 'app-build.log')
    tool(['/usr/bin/codesign', '--force', '--sign', '-', str(app)], 'codesign.log', 30)
    receipt['appExecutableSha256'] = hashlib.sha256((app / 'ContactReference').read_bytes()).hexdigest()
    harness = out / 'harness'
    harness.mkdir()
    shutil.copytree(source / 'SystemUI.xcodeproj', harness / 'SystemUI.xcodeproj')
    (harness / 'SystemUITests.swift').write_text((source / 'ContactReferenceTests.swift').read_text().replace('__APP_ID__', app_id))
    build = ['-project', str(harness / 'SystemUI.xcodeproj'), '-scheme', 'SystemUITests',
             '-derivedDataPath', str(out / 'DerivedData'), 'CODE_SIGNING_ALLOWED=NO']
    tool(['/usr/bin/xcodebuild', 'build-for-testing', *build, '-destination', 'generic/platform=iOS Simulator'], 'test-build.log', 600)
    runtimes = json.loads(tool(sim('list', 'runtimes', '--json'), 'runtimes.log', 30))['runtimes']
    runtime = next(r for r in runtimes if r['identifier'] == args.runtime and r['isAvailable'])
    assert any(t['identifier'] == args.device_type for t in runtime['supportedDeviceTypes'])
    receipt['runtime'] = runtime
    created = tool(sim('create', 'UIKit single contact ' + out.name, args.device_type, args.runtime), 'create.log', 30)
    identifiers = re.findall(r'^([0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12})$', created, re.MULTILINE)
    if len(identifiers) != 1:
        raise RuntimeError('create.log did not report exactly one owned Simulator UUID')
    device = identifiers[0]
    receipt['device'] = {'id': device, 'type': args.device_type, 'runtime': args.runtime}
    tool(sim('boot', device), 'boot-start.log', 60)
    tool(sim('bootstatus', device, '-b'), 'boot.log', 180)
    tool(sim('install', device, str(app)), 'install.log', 120)
    launch = tool(sim('launch', '--stdout=' + str(out / 'app.stdout'), '--stderr=' + str(out / 'app.stderr'),
                      device, app_id), 'launch.log', 90)
    pid = int(re.search(re.escape(app_id) + r': (\d+)', launch).group(1))
    os.kill(pid, 0)
    receipt['pid'] = pid
    env = dict(os.environ, TEST_RUNNER_REFERENCE_PID=str(pid))
    try:
        tool(['/usr/bin/xcodebuild', 'test-without-building', *build, '-destination', 'platform=iOS Simulator,id=' + device,
              '-parallel-testing-enabled', 'NO', '-resultBundlePath', str(out / 'reference.xcresult'),
              '-only-testing:SystemUITests/SystemUITests/testSingleContact'], 'test.log', 600, env)
        receipt['xctestPassed'] = True
    finally:
        observations = re.findall(r'CONTACT_OBSERVATION (\{.*\})', (out / 'test.log').read_text())
        if len(observations) > 1:
            raise RuntimeError('Expected exactly one contact observation')
        if len(observations) == 1:
            observation = json.loads(observations[-1])
            receipt['observation'] = observation
            receipt['completed'] = True
            receipt['contactAccepted'] = observation == {'pid': pid, 'began': 1, 'ended': 1, 'cancelled': 0, 'activations': 1}
except Exception as error:
    receipt['error'] = str(error)
finally:
    if device:
        # After the one case, never before/during its input. Preserve positive as well as failed transport evidence.
        predicate = '(process == "backboardd") OR (process == "testmanagerd") OR (process == "ContactReference")'
        try:
            tool(sim('spawn', device, 'log', 'show', '--last', '15m', '--style', 'compact', '--info', '--debug',
                     '--predicate', predicate), 'simulator-contact.log', 30)
            receipt['systemLogCaptured'] = True
        except Exception as error:
            receipt['systemLogError'] = str(error)
        for action in ['shutdown', 'delete']:
            try:
                tool(sim(action, device), action + '.log', 60)
            except Exception as error:
                receipt.setdefault('cleanupErrors', {})[action] = str(error)
    receipt['finishedAt'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
    print(json.dumps(receipt, indent=2))
raise SystemExit(0 if receipt.get('xctestPassed') and receipt['contactAccepted'] and not receipt.get('cleanupErrors') else 1)
