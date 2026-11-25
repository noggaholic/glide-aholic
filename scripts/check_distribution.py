"""Inspect distributable source and ZIPs for stored instruction patterns.

This is a representation/ISA heuristic, not proof of provenance or a legal
decision. It complements review of additions. Findings never print matched bytes.
The checker is a release tool; private regression tests are not required.
"""
import argparse
import json
import re
import struct
import subprocess
import sys
import zipfile
from pathlib import Path


def instruction_data(data):
    """Recognize common saved-register/stack prologues as ISA operations.

    No original executable sequence, sequence digest or game-specific opcode
    template is stored here. Scalar opcodes describe the processor instruction set.
    """
    data = bytes(data)
    pos = pushes = moves = stack = stores = allocated = instructions = 0
    for _ in range(8):
        if pos >= len(data):
            break
        start = pos
        while pos < len(data) and 0x40 <= data[pos] <= 0x4f:
            pos += 1
        if pos >= len(data):
            break
        op = data[pos]
        pos += 1
        if 0x50 <= op <= 0x57:
            pushes += 1
        elif op in (0x89, 0x8b, 0x8d):
            if pos >= len(data):
                break
            modrm = data[pos]
            pos += 1
            mod, rm = modrm >> 6, modrm & 7
            if mod != 3 and rm == 4:
                if pos >= len(data):
                    break
                sib = data[pos]
                pos += 1
                if sib & 7 == 4:
                    stack += 1
                    if op == 0x89:
                        stores += 1
                if mod == 0 and sib & 7 == 5:
                    pos += 4
            if mod == 1:
                pos += 1
            elif mod == 2 or (mod == 0 and rm == 5):
                pos += 4
            if mod == 3 and rm == 4:
                stack += 1
            moves += 1
        elif op in (0x81, 0x83):
            if pos >= len(data):
                break
            modrm = data[pos]
            pos += 1
            if modrm >> 6 != 3 or modrm & 7 != 4 or (modrm >> 3) & 7 != 5:
                break
            pos += 1 if op == 0x83 else 4
            stack += 1
            allocated += 1
        elif 0xb8 <= op <= 0xbf:
            if pos + 4 >= len(data):
                break
            value = int.from_bytes(data[pos:pos + 4], 'little')
            if 10000 <= value < 1000000 and data[pos + 4] == 0xc3:
                return True
            break
        elif op == 0x85:
            if (pos + 7 < len(data) and data[pos] >> 6 == 3 and
                    0x70 <= data[pos + 1] <= 0x7f and 0xb8 <= data[pos + 3] <= 0xbf and
                    int.from_bytes(data[pos + 4:pos + 8], 'little') <= 65535):
                return True
            break
        elif op == 0xe8:
            pos += 4
        else:
            break
        if pos > len(data) or pos <= start:
            break
        instructions += 1
        if instructions >= 3 and allocated and (moves or pushes >= 2) and pushes + stack >= 2:
            return True
        if instructions >= 2 and stores >= 2:
            return True
        if instructions >= 4 and pushes >= 2 and moves:
            return True
    return False


MASK = re.compile(r'"[x?]{6,}"')
GENERATED_MASK = re.compile(r'std::(?:string|basic_string\s*<\s*char\s*>)[^;\n]{0,160}\([^;\n]*sizeof\([^)]*\)[^;\n]*,\s*\x27x\x27')
NUMBER = r'(?:0[xX][0-9a-fA-F]+|\d+)'
COMMA_RUN = re.compile(r'(?<![\w])' + NUMBER + r'(?:[\s\\]*,[\s\\]*' + NUMBER + r'){5,}')
SPACED_HEX = re.compile(r'(?<![\w])(?:[0-9a-fA-F]{2}|\?\?)(?:[ \t\r\n]+(?:[0-9a-fA-F]{2}|\?\?)){5,}(?![\w])')
ESCAPED_HEX = re.compile(r'(?:\\x[0-9a-fA-F]{2}){6,}')
PACKED_HEX = re.compile(r'["\x27]([0-9a-fA-F]{12,})["\x27]')


def text_findings(text):
    found = set()

    def add(match, kind):
        found.add((text.count('\n', 0, match.start()) + 1, kind))

    for pattern, kind in ((MASK, 'instruction-mask'), (GENERATED_MASK, 'generated-instruction-mask')):
        for match in pattern.finditer(text):
            add(match, kind)
    for match in COMMA_RUN.finditer(text):
        values = [int(s, 16 if s.lower().startswith('0x') else 10) for s in re.findall(NUMBER, match.group())]
        if all(v <= 255 for v in values) and instruction_data(values):
            add(match, 'instruction-initializer')
    for match in SPACED_HEX.finditer(text):
        tokens = match.group().split()
        nearby = text[max(0, match.start() - 200):match.start()].lower()
        if '??' in tokens or 'byte pattern' in nearby or instruction_data(bytes(int(s, 16) for s in tokens)):
            add(match, 'instruction-hex-text')
    for match in ESCAPED_HEX.finditer(text):
        if instruction_data(bytes.fromhex(match.group().replace('\\x', ''))):
            add(match, 'escaped-instruction-data')
    for match in PACKED_HEX.finditer(text):
        value = match.group(1)
        if len(value) % 2 == 0 and instruction_data(bytes.fromhex(value)):
            add(match, 'packed-instruction-data')
    return [{'line': line, 'kind': kind} for line, kind in sorted(found)]


def binary_findings(data):
    if len(data) < 64 or data[:2] != b'MZ':
        return []
    pe = struct.unpack_from('<I', data, 60)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        return [{'kind': 'malformed-PE'}]
    count, optional = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    table = pe + 24 + optional
    if count > 96 or table + count * 40 > len(data):
        return [{'kind': 'malformed-PE'}]
    findings = []
    for index in range(count):
        at = table + index * 40
        flags = struct.unpack_from('<I', data, at + 36)[0]
        size, offset = struct.unpack_from('<II', data, at + 16)
        if offset + size > len(data):
            return [{'kind': 'malformed-PE'}]
        # Generated executable instructions belong to this program. Inspect
        # stored non-executable data, where signature arrays/masks are embedded.
        if flags & 0x20000000 or not size:
            continue
        region = data[offset:offset + size]
        if re.search(rb'[x?]{6,}\x00', region):
            findings.append({'kind': 'embedded-instruction-mask', 'section': index})
        for pos, value in enumerate(region):
            if value in (0x53, 0x55, 0x56, 0x57, 0x85, 0x89, 0x8b) or 0x40 <= value <= 0x4f or 0xb8 <= value <= 0xbf:
                if instruction_data(region[pos:pos + 48]):
                    findings.append({'kind': 'embedded-instruction-data', 'section': index, 'offset': offset + pos})
                    break
    return findings


def inspect(name, data):
    if data[:2] == b'MZ':
        return [{'file': name, **f} for f in binary_findings(data)]
    try:
        text = data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return []
    return [{'file': name, **f} for f in text_findings(text)]


def source_paths(root):
    names = set()
    inventory = root / 'release-files.txt'
    if inventory.is_file():
        names.update(inventory.read_text(encoding='utf-8-sig').splitlines())
    if (root / '.git').exists():
        done = subprocess.run(['git', '-c', 'core.quotepath=false', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard'], capture_output=True, text=True, check=True)
        names.update(done.stdout.splitlines())
    for name in sorted(names):
        path = root / name
        if path.is_file():
            yield name, path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--zip', type=Path, action='append', default=[])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if not args.source and not args.zip:
        parser.error('Select source and/or finished ZIPs')
    findings = []
    files = 0
    if args.source:
        for name, path in source_paths(args.source):
            findings.extend(inspect(name, path.read_bytes()))
            files += 1
    for path in args.zip:
        with zipfile.ZipFile(path) as archive:
            if archive.testzip():
                findings.append({'file': path.name, 'kind': 'ZIP-integrity'})
            for entry in archive.infolist():
                if not entry.is_dir():
                    findings.extend(inspect(path.name + ':' + entry.filename, archive.read(entry)))
                    files += 1
    result = {'files': files, 'findings': findings, 'passed': not findings}
    if args.report:
        args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    for finding in findings:
        print(json.dumps(finding))
    print(f'Distribution instruction-pattern check: {files} files, {len(findings)} findings.')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
