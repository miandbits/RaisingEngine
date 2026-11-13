"""Verify that only compiled or generated app metadata differs across the two builds."""
import json
from pathlib import Path

from files import digest

root = Path('.cache/ios-app-build-comparison')
a, b = root / 'A', root / 'B'
assert (a / 'arm.txt').read_text().strip() == 'A'
assert (b / 'arm.txt').read_text().strip() == 'B'
for name in ('source.txt', 'tree.txt', 'prepared.sha', 'input-before.json', 'input-after.json', 'ios-build-original.json', 'game.sha'):
    assert (a / name).read_bytes() == (b / name).read_bytes(), name
assert (a / 'executable.sha').read_bytes() != (b / 'executable.sha').read_bytes()

def without_build_metadata(folder):
    plist = json.loads((folder / 'bundle-info.json').read_bytes())
    removed = {}
    for key in list(plist):
        if key in {'BuildMachineOSBuild'} or key.startswith('DT'):
            removed[key] = plist.pop(key)
    return plist, removed

a_plist, a_generated = without_build_metadata(a)
b_plist, b_generated = without_build_metadata(b)
assert a_plist == b_plist, 'Bundle behavior and identity keys differ'
compiled = {'RaisingPlayer', 'Info.plist', 'Assets.car', '_CodeSignature', '_CodeSignature/CodeResources'}

def resources(folder):
    entries = json.loads((folder / 'app-manifest.json').read_bytes())
    return [item for item in entries if item['path'] not in compiled]

a_resources = resources(a)
assert a_resources == resources(b), 'App resources differ'
def build_identity(folder, generated):
    return {
        'executableSha256': (folder / 'executable.sha').read_text().strip(),
        'bundleArchiveSha256': digest(folder / 'starter-app.tar'),
        'bundleManifestSha256': digest(folder / 'app-manifest.json'),
        'generatedPlist': generated,
    }


print(json.dumps({
    'source': (a / 'source.txt').read_text().strip(),
    'tree': (a / 'tree.txt').read_text().strip(),
    'preparedArchiveSha256': (a / 'prepared.sha').read_text().strip(),
    'gameJsonSha256': (a / 'game.sha').read_text().strip(),
    'resourceEntries': len(a_resources),
    'A': build_identity(a, a_generated),
    'B': build_identity(b, b_generated),
    'scope': 'Common resources and bundle behavior keys. Xcode toolchains and producer VM images differ.',
}, indent=2))
