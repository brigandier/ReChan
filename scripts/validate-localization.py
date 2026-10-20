"""Validate shipped catalogs and subtitle resources without original game data."""
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]

def catalog(path):
    result = {}
    pending = None
    for number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        match = re.fullmatch(r'\[([^\]]+)\](?:\s*//.*)?', line)
        if match:
            assert pending is None, (path, number, 'missing text', pending)
            pending = match[1]
            assert pending not in result, (path, number, 'duplicate', pending)
            assert pending.startswith('0x') or len(pending) <= 16, (path, pending)
        else:
            assert pending, (path, number, 'orphan text')
            result[pending] = line
            pending = None
    assert pending is None, (path, 'missing final text')
    return result

def font_characters(path):
    data = path.read_bytes()
    u16 = lambda pos: struct.unpack_from('>H', data, pos)[0]
    u32 = lambda pos: struct.unpack_from('>I', data, pos)[0]
    cmap = next(u32(12+i*16+8) for i in range(u16(4)) if data[12+i*16:16+i*16] == b'cmap')
    characters = set()
    for i in range(u16(cmap+2)):
        record = cmap + 4 + i*8
        if u16(record) not in (0, 3):
            continue
        start = cmap + u32(record+4)
        if u16(start) != 4:
            continue
        count = u16(start+6)//2
        end_pos = start+14
        begin_pos = end_pos+count*2+2
        delta_pos = begin_pos+count*2
        range_pos = delta_pos+count*2
        for seg in range(count):
            for cp in range(u16(begin_pos+seg*2), min(u16(end_pos+seg*2), 0xfffe)+1):
                offset = u16(range_pos+seg*2)
                glyph = u16(range_pos+seg*2+offset+2*(cp-u16(begin_pos+seg*2))) if offset else cp
                if not offset or glyph:
                    glyph = (glyph+u16(delta_pos+seg*2)) & 0xffff
                if glyph:
                    characters.add(chr(cp))
    return characters

def main():
    base = catalog(ROOT/'res/pc/text/english.txt')
    controls = re.compile(r'%(?:\d+\$)?[-+0 #]*\d*(?:\.\d+)?[a-zA-Z%]|<[^>]+>')
    for path in sorted((ROOT/'res/pc/text').glob('*.txt')):
        entries = catalog(path)
        assert entries.keys() == base.keys(), (path, 'token mismatch', entries.keys() ^ base.keys())
        for key, text in entries.items():
            assert controls.findall(text) == controls.findall(base[key]), (path, key, 'format mismatch')
            assert not any(ord(c) < 32 or c == '\ufffd' for c in text), (path, key, 'invalid characters')
        print(f'PASS {path.name}: {len(entries)} identifiers, placeholders and action tokens')
    pt = catalog(ROOT/'res/pc/text/portuguese.txt')
    required = set('áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ')
    for name in ('YIKES!__.TTF', 'Roboto-Regular.ttf', 'Europa Grotesk SH DemBol.ttf'):
        supported = font_characters(ROOT/'res/pc/fonts'/name)
        missing = required-supported
        assert not missing, (name, 'missing Portuguese glyphs', sorted(missing))
        print(f'PASS {name}: all requested Portuguese accents')
    expanded = [(len(value), key) for key, value in pt.items() if len(value) > len(base[key])*1.4 and len(value) > 25]
    print('VISUAL REVIEW (length expansion, not failures):', sorted(expanded, reverse=True))

if __name__ == '__main__':
    main()
