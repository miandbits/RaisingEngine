"""Inventory the exact regular files and directories in an exported build input or bundle."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import tarfile


def digest(file):
    hash_value = hashlib.sha256()
    with file.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            hash_value.update(chunk)
    return hash_value.hexdigest()


def check_archive(archive, top):
    """Validate every member before unpacking a hash-verified run artifact."""
    seen = set()
    directories = set()
    with tarfile.open(archive, 'r:') as data:
        for member in data:
            name = PurePosixPath(member.name)
            assert name.parts and name.parts[0] == top, member.name
            assert member.name.rstrip('/') == name.as_posix() and '..' not in name.parts, member.name
            assert name not in seen and (member.isdir() or member.isreg()), member.name
            assert not member.mode & 0o7000, member.name
            assert len(name.parts) == 1 or name.parent in directories, member.name
            seen.add(name)
            if member.isdir():
                directories.add(name)
    assert PurePosixPath(top) in directories and len(seen) > 1


def inventory(root, prepared):
    assert stat.S_ISDIR(root.lstat().st_mode), root
    entries = []

    def scan(directory):
        for item in sorted(directory.iterdir()):
            relative = item.relative_to(root).as_posix()
            if prepared and item.parent == root and relative in {'ios-build.json', 'xcode-configure.log', 'xcode-build.log', 'build-iphonesimulator'}:
                continue
            details = item.lstat()
            mode = stat.S_IMODE(details.st_mode)
            assert not mode & 0o7000, relative
            if stat.S_ISDIR(details.st_mode):
                entries.append({'path': relative, 'kind': 'directory', 'mode': mode})
                scan(item)
            elif stat.S_ISREG(details.st_mode):
                entries.append({'path': relative, 'kind': 'file', 'mode': mode, 'bytes': details.st_size, 'sha256': digest(item)})
            else:
                raise ValueError(f'Unexpected file type: {relative}')

    scan(root)
    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('operation', choices=('prepared', 'bundle', 'check-archive'))
    parser.add_argument('path', type=Path)
    parser.add_argument('top', nargs='?')
    args = parser.parse_args()
    if args.operation == 'check-archive':
        assert args.top
        check_archive(args.path, args.top)
    else:
        print(json.dumps(inventory(args.path, args.operation == 'prepared'), separators=(',', ':')))


if __name__ == '__main__':
    main()
