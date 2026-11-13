#!/usr/bin/env python3
"""Rebuild the isolated Qt Cocoa repair without installing it or modifying Qt."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--work-dir', type=Path, required=True, help='New, nonexistent output directory')
parser.add_argument('--qt-prefix', type=Path, default=Path('/opt/homebrew/opt/qtbase'))
parser.add_argument('--cmake', default='/opt/homebrew/bin/cmake')
parser.add_argument('--ninja', default='/opt/homebrew/bin/ninja')
parser.add_argument('--jobs', type=int, default=3)
args = parser.parse_args()
kit = Path(__file__).resolve().parent
work = args.work_dir.resolve()
if platform.system() != 'Darwin' or platform.machine() != 'arm64':
    parser.error('This narrowly verified recipe is for macOS arm64 only')
if work.exists():
    parser.error('work-dir must not exist; existing outputs are never replaced')
if not 1 <= args.jobs <= 16:
    parser.error('jobs must be between 1 and 16')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

checksums = json.loads((kit / 'source-checksums.json').read_text())
for relative, expected in checksums.items():
    if sha(kit / relative) != expected:
        raise SystemExit('Source-kit checksum mismatch: ' + relative)

installed = json.loads((kit / 'installed-kit.json').read_text())
for name in ['QtCore', 'QtGui', 'QtWidgets']:
    path = args.qt_prefix / 'lib' / (name + '.framework') / 'Versions/A' / name
    expected = next(x['sha256'] for x in installed['installedBinaries']
                    if x['path'].endswith('/' + name))
    if sha(path) != expected:
        raise SystemExit('Different Qt kit binary: ' + name + '. Revalidate compatibility before adapting this recipe.')

work.mkdir(parents=True)
commands = []

def run(command, log):
    commands.append([str(x) for x in command])
    with (work / log).open('w') as output:
        subprocess.run(commands[-1], stdout=output, stderr=subprocess.STDOUT, check=True)

def extract(name):
    destination = work / 'sources'
    destination.mkdir(exist_ok=True)
    with tarfile.open(kit / 'downloads' / name) as archive:
        for member in archive:
            path = Path(member.name)
            if path.is_absolute() or '..' in path.parts:
                raise SystemExit('Unsafe archive entry')
            if member.issym() or member.islnk():
                link = Path(member.linkname)
                relative_target = path.parent / link if member.issym() else link
                normalized = Path(os.path.normpath(str(relative_target)))
                if link.is_absolute() or normalized == Path('..') or normalized.parts[0] == '..':
                    raise SystemExit('Unsafe archive link')
        archive.extractall(destination)

for name in ['qtbase-everywhere-src-6.12.0.tar.xz', 'MoltenVK-v1.4.2.tar.gz', 'Vulkan-Headers-1.4.363.0.tar.gz']:
    extract(name)
source = work / 'sources/qtbase-everywhere-src-6.12.0'
run(['/usr/bin/patch', '--batch', '--forward', '-d', source, '-p1', '-i',
     kit / 'downloads/b74d6cb3c5e8418602494ae549b60839d88ef858.patch'], 'patch.log')
include = work / 'compile-headers'
include.mkdir()
shutil.copytree(work / 'sources/MoltenVK-1.4.2/MoltenVK/MoltenVK/API', include / 'MoltenVK')
for name in ['vulkan', 'vk_video']:
    shutil.copytree(work / 'sources/Vulkan-Headers-vulkan-sdk-1.4.363.0/include' / name, include / name)
run([args.cmake, '-S', kit / 'driver', '-B', work / 'build', '-G', 'Ninja',
     '-DCMAKE_MAKE_PROGRAM=' + args.ninja, '-DCMAKE_PREFIX_PATH=' + str(args.qt_prefix),
     '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
     '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.4', '-DQTBASE_SOURCE_DIR=' + str(source),
     '-DCOMPILE_HEADERS_DIR=' + str(include), '-DCMAKE_INSTALL_PREFIX=' + str(work / 'unused-install')], 'configure.log')
run([args.cmake, '--build', work / 'build', '--parallel', str(args.jobs)], 'build.log')
plugin = work / 'build/plugins/platforms/libqcocoa.dylib'
(work / 'build-receipt.json').write_text(json.dumps({
    'ok': True, 'commands': commands, 'plugin': str(plugin), 'sha256': sha(plugin),
    'installed': False, 'nativeAcceptancePending': True,
    'note': 'Build outputs contain absolute Qt dependencies; relocate only an isolated app copy and re-sign it.'
}, indent=2) + '\n')
print(plugin)
