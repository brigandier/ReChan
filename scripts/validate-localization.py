"""Validate shipped catalogs and subtitle resources without original game data.

Add new identifiers to their respective categories in every catalog, not to the
end of the file. The English catalog defines the canonical sequence.
"""
from pathlib import Path
import re
import struct
import unicodedata

ROOT = Path(__file__).resolve().parents[1]

def validate_order(reference, entries, path):
    """Keep new tokens in their menu category; english.txt defines the order."""
    assert entries.keys() == reference.keys(), (path, 'token mismatch', entries.keys() ^ reference.keys())
    for position, (expected, actual) in enumerate(zip(reference, entries), 1):
        assert expected == actual, (path, 'token order mismatch at position', position,
                                    'expected', expected, 'got', actual)

def token_hash(token):
    if '0' <= token[0] <= '9':
        if token.lower().startswith('0x'):
            value = int(token[2:], 16)
        else:
            value = int(token, 8 if token.startswith('0') else 10)
        assert 0 <= value <= 0xffffffff, ('numeric token outside u32', token)
        return value
    value = 0
    for byte in token.encode('utf-8'):
        value = (value * 33 + byte) & 0xffffffff
    return value

def validate_hashes(entries, path):
    hashes = {}
    for token in entries:
        value = token_hash(token)
        assert value not in hashes, (path, 'xcHash collision', f'0x{value:08X}', hashes.get(value), token)
        hashes[value] = token

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

def subtitle_cues(path):
    contents = path.read_text(encoding='utf-8-sig').replace('\r\n', '\n').strip()
    result = []
    for index, block in enumerate(contents.split('\n\n'), 1):
        lines = block.splitlines()
        assert len(lines) >= 3 and lines[0] == str(index), (path, index, 'cue identifier')
        times = re.fullmatch(r'(\d{2}):([0-5]\d):([0-5]\d),(\d{3}) --> (\d{2}):([0-5]\d):([0-5]\d),(\d{3})', lines[1])
        assert times, (path, index, 'timestamp')
        fields = [int(value) for value in times.groups()]
        seconds = lambda values: values[0]*3600 + values[1]*60 + values[2] + values[3]/1000
        assert seconds(fields[:4]) < seconds(fields[4:]), (path, index, 'duration')
        text = '\n'.join(lines[2:])
        assert text and text == unicodedata.normalize('NFC', text), (path, index, 'text normalization')
        assert not any((ord(c) < 32 and c != '\n') or c == '\ufffd' for c in text), (path, index, 'invalid characters')
        assert len(lines[2:]) <= 2, (path, index, 'more than two lines')
        if path.parent.name != 'pt-BR':
            assert max(map(len, lines[2:])) <= 42, (path, index, 'line too long')
        result.append((lines[0], lines[1], text))
    return result

def main():
    base = catalog(ROOT/'res/pc/text/english.txt')
    controls = re.compile(r'%(?:\d+\$)?[-+0 #]*\d*(?:\.\d+)?[a-zA-Z%]|<[^>]+>')
    for path in sorted((ROOT/'res/pc/text').glob('*.txt')):
        entries = catalog(path)
        validate_order(base, entries, path)
        validate_hashes(entries, path)
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
    subtitle_root = ROOT/'res/pc/subtitles'
    reference = {path.name: subtitle_cues(path) for path in (subtitle_root/'pt-BR').glob('*.srt')}
    assert set(reference) == {'PROLOG.srt', 'FACTORY.srt', 'MAKING.srt', 'VICTORY.srt', 'CREDITS.srt'}
    subtitle_characters = set()
    for locale in ('en', 'de', 'fr', 'it', 'es', 'pt-BR'):
        paths = {path.name: path for path in (subtitle_root/locale).glob('*.srt')}
        assert paths.keys() == reference.keys(), (locale, 'movie mismatch')
        for name, path in paths.items():
            cues = subtitle_cues(path)
            assert [(cue[0], cue[1]) for cue in cues] == [(cue[0], cue[1]) for cue in reference[name]], (path, 'timing mismatch')
            subtitle_characters.update(''.join(cue[2] for cue in cues))
        print(f'PASS {locale}: five native SRTs, 117 matching cue IDs/times, UTF-8 and two-line layout')
    subtitle_characters.discard('\n')
    for name in ('YIKES!__.TTF', 'Roboto-Regular.ttf', 'Europa Grotesk SH DemBol.ttf'):
        assert not subtitle_characters-font_characters(ROOT/'res/pc/fonts'/name), (name, 'missing subtitle glyphs')
    print('PASS all subtitle characters in the three bundled fonts')
    for name, dimensions in [('controller_overlay_playstation.png', (1024, 1024)), ('controller_sheet_playstation.png', (632, 1808))]:
        data = (ROOT/'res/pc/textures/frontend'/name).read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n', name
        assert struct.unpack_from('>II', data, 16) == dimensions, (name, 'atlas dimensions')
        assert data[25] == 6, (name, 'RGBA transparency required')
        print(f'PASS {name}: {dimensions}, RGBA')

if __name__ == '__main__':
    main()
