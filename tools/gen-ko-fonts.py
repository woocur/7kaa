#!/usr/bin/env python3
"""Build the Korean font set (FNT_*_KO.RES) for 7kaa.

Each output font keeps the original glyphs for codes 33-255 and adds
glyphs for every code point from 256 up to 0xFFEF, either from the Galmuri
BDF pixel fonts (OFL, https://github.com/quiple/galmuri) or from a TrueType
font rasterized without anti-aliasing (e.g. Mabinogi Classic / 마비옛체).
The game decodes text as UTF-8 when a font covers more than 8-bit codes.

Usage: gen-ko-fonts.py <7kaa data/RESOURCE dir> <galmuri dist dir> <output dir>
       gen-ko-fonts.py --ttf <font.ttf|.woff2> <7kaa data/RESOURCE dir> <output dir>
"""
import os
import struct
import sys

LAST_CHAR = 0xFFEF
TRANSPARENT = 255

# font name: (bdf file, main color, shadow color or None)
# The colors are the palette indices the original Latin glyphs use.
FONTS = {
    'STD':  ('Galmuri11.bdf',      0,   None),
    'SAN':  ('Galmuri11-Bold.bdf', 113, 95),
    'MID':  ('Galmuri11-Bold.bdf', 0,   95),
    'SMAL': ('Galmuri9.bdf',       0,   None),
    'NEWS': ('Galmuri11-Bold.bdf', 95,  0),
    'CASA': ('Galmuri14.bdf',      219, 95),
}
FALLBACK_BDF = 'Galmuri11.bdf'   # for code points the bold face lacks

# font name: pixel size used when rasterizing a TrueType font
TTF_SIZES = {'STD': 13, 'SAN': 13, 'MID': 13, 'SMAL': 12, 'NEWS': 13, 'CASA': 16}


def read_bdf(path):
    glyphs = {}
    with open(path, encoding='latin-1') as f:
        lines = f.read().split('\n')
    i = 0
    while i < len(lines):
        if lines[i].startswith('STARTCHAR'):
            enc = adv = None
            bbx = None
            rows = []
            i += 1
            while not lines[i].startswith('ENDCHAR'):
                ln = lines[i]
                if ln.startswith('ENCODING'):
                    enc = int(ln.split()[1])
                elif ln.startswith('DWIDTH'):
                    adv = int(ln.split()[1])
                elif ln.startswith('BBX'):
                    bbx = tuple(int(v) for v in ln.split()[1:5])
                elif ln.startswith('BITMAP'):
                    i += 1
                    while not lines[i].startswith('ENDCHAR'):
                        rows.append(lines[i].strip())
                        i += 1
                    continue
                i += 1
            if enc is not None and enc >= 0 and bbx:
                w, h, xo, yo = bbx
                pix = [[(int(r, 16) >> (len(r) * 4 - 1 - x)) & 1 for x in range(w)] for r in rows]
                glyphs[enc] = (adv, w, h, xo, yo, pix)
        i += 1
    return glyphs


def read_ttf(path, size):
    """Rasterize every non-Latin-1 glyph of a TrueType font into BDF-like tuples."""
    from fontTools.ttLib import TTFont
    from PIL import Image, ImageDraw, ImageFont
    tt = TTFont(path)
    codes = [c for c in tt.getBestCmap() if 0xFF < c <= LAST_CHAR]
    if path.endswith('.woff2') or path.endswith('.woff'):
        import io
        tt.flavor = None
        buf = io.BytesIO()
        tt.save(buf)
        buf.seek(0)
        font = ImageFont.truetype(buf, size)
    else:
        font = ImageFont.truetype(path, size)
    ascent = font.getmetrics()[0]
    pad = size
    glyphs = {}
    for code in codes:
        ch = chr(code)
        im = Image.new('1', (size * 3, size * 3), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = '1'
        d.text((pad, pad), ch, font=font, fill=1)
        bb = im.getbbox()
        if not bb:
            continue
        x1, y1, x2, y2 = bb
        x1 = min(x1, pad)          # keep the left side bearing
        w, h = x2 - x1, y2 - y1
        pix = [[1 if im.getpixel((x1 + x, y1 + y)) else 0 for x in range(w)] for y in range(h)]
        yo = (pad + ascent) - y2   # bottom of the bitmap relative to the baseline
        glyphs[code] = (int(round(font.getlength(ch))), w, h, 0, yo, pix)
    return glyphs


def read_font(path):
    data = open(path, 'rb').read()
    mw, mh, sh, fc, lc = struct.unpack('<5H', data[:10])
    n = lc - fc + 1
    infos = [struct.unpack('<bBBI', data[10 + i * 7:17 + i * 7]) for i in range(n)]
    bitmap = data[10 + n * 7:]
    return (mw, mh, sh, fc, lc), infos, bitmap


def make_glyph(g, base, main, shadow):
    """Return (offset_y, width, height, bitmap bytes) for a BDF glyph."""
    adv, w, h, xo, yo, pix = g
    width = max(w + max(xo, 0), 1)
    top = base - (yo + h)
    extra = 1 if shadow is not None else 0
    gw, gh = width + extra, h + extra
    grid = [[TRANSPARENT] * gw for _ in range(gh)]
    if shadow is not None:
        for y in range(h):
            for x in range(w):
                if pix[y][x]:
                    for dx, dy in ((1, 0), (0, 1), (1, 1)):
                        grid[y + dy][x + max(xo, 0) + dx] = shadow
    for y in range(h):
        for x in range(w):
            if pix[y][x]:
                grid[y][x + max(xo, 0)] = main
    if top < 0:
        grid = grid[-top:]
        gh += top
        top = 0
    body = bytes(v for row in grid for v in row)
    return top, gw, gh, struct.pack('<HH', gw, gh) + body


def build(name, res_dir, bdf_dir, out_dir, cache, ttf=None):
    bdf_name, main, shadow = FONTS[name]
    if ttf:
        key = (ttf, TTF_SIZES[name])
        if key not in cache:
            cache[key] = read_ttf(ttf, TTF_SIZES[name])
        bdf = fallback = cache[key]
    else:
        for b in (bdf_name, FALLBACK_BDF):
            if b not in cache:
                cache[b] = read_bdf(os.path.join(bdf_dir, b))
        bdf, fallback = cache[bdf_name], cache[FALLBACK_BDF]
        if bdf_name.startswith('Galmuri9') or bdf_name.startswith('Galmuri14'):
            fallback = bdf

    (mw, mh, sh, fc, lc), infos, bitmap = read_font(os.path.join(res_dir, 'FNT_%s.RES' % name))
    # baseline of the original font: bottom of 'A'
    a = infos[ord('A') - fc]
    base = a[0] + a[2]

    out_infos = []
    out_bitmap = bytearray()
    max_w = mw
    for code in range(fc, LAST_CHAR + 1):
        if code <= lc:
            oy, w, h, off = infos[code - fc]
            if w or h:
                gw, gh = struct.unpack('<HH', bitmap[off:off + 4])
                blob = bitmap[off:off + 4 + gw * gh]
                out_infos.append((oy, w, h, len(out_bitmap)))
                out_bitmap += blob
            else:
                out_infos.append((0, 0, 0, 0))
            continue
        g = bdf.get(code) or fallback.get(code)
        if not g or not any(any(r) for r in g[5]):
            out_infos.append((0, 0, 0, 0))
            continue
        top, gw, gh, blob = make_glyph(g, base, main, shadow)
        out_infos.append((top, gw, gh, len(out_bitmap)))
        out_bitmap += blob
        max_w = max(max_w, gw)

    out = bytearray(struct.pack('<5H', max_w, mh, sh, fc, LAST_CHAR))
    for oy, w, h, off in out_infos:
        out += struct.pack('<bBBI', oy, w, h, off)
    out += out_bitmap
    path = os.path.join(out_dir, 'FNT_%s_KO.RES' % name)
    open(path, 'wb').write(out)
    return path, len(out)


def main():
    ttf = None
    args = sys.argv[1:]
    if args[0] == '--ttf':
        ttf = args[1]
        res_dir, out_dir = args[2:4]
        bdf_dir = None
    else:
        res_dir, bdf_dir, out_dir = args[0:3]
    cache = {}
    for name in FONTS:
        path, size = build(name, res_dir, bdf_dir, out_dir, cache, ttf)
        print('%s %d' % (path, size))


if __name__ == '__main__':
    main()
