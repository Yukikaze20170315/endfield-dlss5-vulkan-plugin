"""Rebuild the Vulkan NR transport shader; writes only inside this project."""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
GENERIC = ROOT / 'src/unified-20261011/generic'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--slangc', type=Path, required=True)
    args = parser.parse_args()
    output = ROOT / 'builds/unified-20261011/nr-resolution-tests'
    output.mkdir(parents=True, exist_ok=True)
    shader = 'vk_look_upsample'
    source = GENERIC / f'src/addons/dlss5/shaders/{shader}.comp.slang'
    binary = output / f'{shader}.spv'
    command = [str(args.slangc.resolve()), str(source), '-stage', 'compute',
               '-O3', '-g0', '-entry', 'main', '-Wno-30056', '-Wno-15205',
               '-DNDEBUG', '-target', 'spirv', '-o', str(binary)]
    env = os.environ.copy()
    env.update(TEMP=str(output), TMP=str(output))
    subprocess.run(command, cwd=output, env=env, check=True)
    data = binary.read_bytes()
    words = struct.unpack(f'<{len(data) // 4}I', data)
    assert words[0] == 0x07230203, 'Invalid SPIR-V magic'
    capabilities, bindings, local_sizes = set(), set(), []
    offset = 5
    while offset < len(words):
        count, opcode = words[offset] >> 16, words[offset] & 0xffff
        assert count > 0 and offset + count <= len(words)
        operands = words[offset + 1:offset + count]
        if opcode == 17:  # OpCapability
            capabilities.add(operands[0])
        if opcode == 71 and operands[1] == 33:  # OpDecorate Binding
            bindings.add(operands[2])
        if opcode == 16 and operands[1] == 17:  # LocalSize
            local_sizes.append(tuple(operands[2:5]))
        offset += count
    assert not capabilities.intersection({9, 22}), 'Float16/Int16 requires unavailable game features'
    assert bindings == {0, 1, 2, 10, 11}, f'Unexpected transport descriptors: {bindings}'
    assert local_sizes == [(16, 16, 1)], local_sizes
    symbol = '__' + shader
    body = '\n'.join('    ' + ', '.join(str(n) for n in data[i:i + 16]) + ','
                     for i in range(0, len(data), 16))
    header = (f'#pragma once\n#ifndef {symbol}_EMBED_FILE\n#define {symbol}_EMBED_FILE\n'
              '#include <cstdint>\n#include <span>\n'
              f'inline constexpr std::uint8_t {symbol}_base[] = {{\n{body}\n}};\n'
              f'inline constexpr std::span<const std::uint8_t> {symbol}{{{symbol}_base}};\n#endif\n')
    target = GENERIC / f'generated/embed/{shader}.h'
    if target.exists():
        previous = target.read_text(encoding='utf-8')
        if previous == header:
            print('Header already matches compiler output')
        else:
            target.write_text(header, encoding='utf-8', newline='\n')
    else:
        target.write_text(header, encoding='utf-8', newline='\n')
    print(f'{shader}: {len(data)} bytes, SHA256={hashlib.sha256(data).hexdigest()}')
    print(f'SPIR-V structural checks passed: capabilities={sorted(capabilities)}, bindings={sorted(bindings)}')


if __name__ == '__main__':
    main()
