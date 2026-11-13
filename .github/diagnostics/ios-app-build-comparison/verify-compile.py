"""Bind the exact SDL compile and player link blocks in the Xcode build transcript."""
from pathlib import Path
import sys


def verified_blocks(log, scene, executable, toolchain, sdk):
    results = {}
    lines = log.splitlines()
    for prefix, artifact, compiler, output in (
        ('CompileC ', scene, 'clang', 'sdl-view-frame-compile.txt'),
        ('Ld ', executable, 'clang++', 'app-link.txt'),
    ):
        matching = [index for index, line in enumerate(lines) if line.startswith(prefix) and f' {artifact} ' in line]
        assert len(matching) == 1, f'Expected one {prefix.strip()} block for {artifact}'
        start = matching[0]
        end = start + 1
        while end < len(lines) and lines[end].startswith((' ', '\t')):
            end += 1
        block = lines[start:end]
        commands = [line.strip() for line in block[1:] if line.lstrip().startswith(f'{toolchain}/{compiler} ')]
        assert len(commands) == 1, f'Expected one actual {compiler} command for {artifact}'
        command = commands[0]
        assert '-target arm64-apple-ios16.0-simulator ' in command, command
        assert f'-isysroot {sdk} ' in command, command
        assert f' {artifact} ' in command or command.endswith(f' {artifact}'), command
        results[output] = '\n'.join(block) + '\n'
    return results


if __name__ == '__main__':
    log, scene, executable, toolchain, sdk, output = sys.argv[1:]
    for name, block in verified_blocks(Path(log).read_text(), scene, executable, toolchain, sdk).items():
        (Path(output) / name).write_text(block)
