#!/usr/bin/env python3
"""
compare_bmp.py - diff two 24-bit BMPs and say WHERE they differ.

    python tools/compare_bmp.py a.bmp b.bmp
    python tools/compare_bmp.py a.bmp b.bmp --quiet     # just the verdict

Built for the PPU lessons:

  * L24 compares your dmg-acid2 frame against the reference image. The tolerance is
    zero pixels, and the interesting output is the bounding box: a difference confined
    to one 8-pixel band is a tile-row problem, while a difference across a whole region
    is a layer problem.
  * L25 compares two frames of a scrolling game to confirm that the differences are
    confined to the play area and the status bar is byte-identical.

Reads and writes nothing except the two files. Standard library only. Exit code 0 when
identical, 1 when they differ, 2 on a malformed or mismatched pair.
"""

import argparse
import struct
import sys


def read_bmp(path):
    """Return (width, height, pixels) for a 24-bit uncompressed BMP, top-down rows."""
    with open(path, "rb") as fh:
        data = fh.read()

    if len(data) < 54 or data[0:2] != b"BM":
        raise ValueError("%s: not a BMP (bad magic)" % path)

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    header_size = struct.unpack_from("<I", data, 14)[0]
    if header_size < 40:
        raise ValueError("%s: unsupported BMP header size %d" % (path, header_size))

    width, height = struct.unpack_from("<ii", data, 18)
    planes, bpp = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]

    if planes != 1 or bpp != 24 or compression != 0:
        raise ValueError("%s: expected a 24-bit uncompressed BMP (planes=%d bpp=%d comp=%d)"
                         % (path, planes, bpp, compression))

    bottom_up = height > 0
    height = abs(height)

    row_bytes = width * 3
    pad = (4 - (row_bytes % 4)) % 4
    stride = row_bytes + pad
    if len(data) < pixel_offset + stride * height:
        raise ValueError("%s: truncated pixel data" % path)

    rows = []
    for row in range(height):
        start = pixel_offset + row * stride
        rows.append(data[start:start + row_bytes])
    if bottom_up:
        rows.reverse()

    return width, height, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("a", help="the frame you produced")
    ap.add_argument("b", help="the reference frame")
    ap.add_argument("--quiet", action="store_true", help="print only the verdict")
    args = ap.parse_args()

    try:
        wa, ha, pa = read_bmp(args.a)
        wb, hb, pb = read_bmp(args.b)
    except (OSError, ValueError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2

    if (wa, ha) != (wb, hb):
        print("error: sizes differ: %s is %dx%d, %s is %dx%d"
              % (args.a, wa, ha, args.b, wb, hb), file=sys.stderr)
        return 2

    differing = 0
    total_delta = 0
    worst = 0
    x0, y0, x1, y1 = wa, ha, -1, -1

    for y in range(ha):
        ra, rb = pa[y], pb[y]
        if ra == rb:
            continue
        for x in range(wa):
            i = x * 3
            if ra[i:i + 3] == rb[i:i + 3]:
                continue
            differing += 1
            d = max(abs(ra[i] - rb[i]), abs(ra[i + 1] - rb[i + 1]),
                    abs(ra[i + 2] - rb[i + 2]))
            total_delta += d
            worst = max(worst, d)
            x0 = min(x0, x)
            y0 = min(y0, y)
            x1 = max(x1, x)
            y1 = max(y1, y)

    if differing == 0:
        print("identical: %s == %s (%dx%d, 0 differing pixels)" % (args.a, args.b, wa, ha))
        return 0

    print("%d of %d pixels differ (%.2f%%), worst channel delta %d"
          % (differing, wa * ha, 100.0 * differing / (wa * ha), worst))
    if not args.quiet:
        print("  bounding box: x %d..%d, y %d..%d" % (x0, x1, y0, y1))
        print("  average channel delta over differing pixels: %.1f"
              % (total_delta / float(differing)))
        # A bounding box that hugs one tile row or one sprite is a much better clue
        # than the raw count, so say which shape it looks like.
        bw, bh = x1 - x0 + 1, y1 - y0 + 1
        if bw <= 16 and bh <= 16:
            print("  shape: a small block -> suspect one sprite or one tile")
        elif bh <= 8:
            print("  shape: a horizontal band <= 8 pixels tall -> suspect one tile row")
        elif bw <= 8:
            print("  shape: a vertical band <= 8 pixels wide -> suspect one tile column")
        else:
            print("  shape: a large region -> suspect a whole layer (sprites, window or BG)")

    return 1


if __name__ == "__main__":
    sys.exit(main())
