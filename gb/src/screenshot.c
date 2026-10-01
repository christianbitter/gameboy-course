/*
 * screenshot.c - dump the framebuffer to a file.
 *
 * PROVIDED INFRASTRUCTURE. BMP opens in any Windows viewer, so this is your
 * window onto the PPU until you wire up SDL2 in M11.
 */
#include "gb/gb.h"

/* Classic DMG green. Index = shade 0..3 (0 = lightest). */
const u8 GB_SHADES[4][3] = {
    { 0xE0, 0xF8, 0xD0 },
    { 0x88, 0xC0, 0x70 },
    { 0x34, 0x68, 0x56 },
    { 0x08, 0x18, 0x20 },
};

static u8 shade_rgb(u8 index, int channel)
{
    if (index > 3) index = 3;
    return GB_SHADES[index][channel];
}

/* 24-bit uncompressed BMP. Rows are stored bottom-up, padded to 4 bytes. */
void gb_write_bmp(const char *path, const u8 *framebuffer)
{
    const int w = GB_SCREEN_W;
    const int h = GB_SCREEN_H;
    const int row_bytes = w * 3;
    const int pad = (4 - (row_bytes % 4)) % 4;
    const u32 pixel_bytes = (u32)((row_bytes + pad) * h);
    const u32 file_size = 54 + pixel_bytes;

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot write '%s'\n", path);
        return;
    }

    u8 header[54];
    memset(header, 0, sizeof header);
    header[0] = 'B'; header[1] = 'M';
    header[2] = (u8)(file_size);
    header[3] = (u8)(file_size >> 8);
    header[4] = (u8)(file_size >> 16);
    header[5] = (u8)(file_size >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = (u8)(w);
    header[19] = (u8)(w >> 8);
    header[20] = (u8)(w >> 16);
    header[21] = (u8)(w >> 24);
    header[22] = (u8)(h);
    header[23] = (u8)(h >> 8);
    header[24] = (u8)(h >> 16);
    header[25] = (u8)(h >> 24);
    header[26] = 1;             /* planes */
    header[28] = 24;            /* bits per pixel */
    fwrite(header, 1, sizeof header, f);

    const u8 zero = 0;
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            u8 s = framebuffer[y * w + x];
            u8 px[3] = { shade_rgb(s, 2), shade_rgb(s, 1), shade_rgb(s, 0) };
            fwrite(px, 1, 3, f);
        }
        for (int p = 0; p < pad; p++) fwrite(&zero, 1, 1, f);
    }
    fclose(f);
}

void gb_write_ppm(const char *path, const u8 *framebuffer)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot write '%s'\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", GB_SCREEN_W, GB_SCREEN_H);
    for (int i = 0; i < GB_FB_SIZE; i++) {
        u8 s = framebuffer[i];
        u8 px[3] = { shade_rgb(s, 0), shade_rgb(s, 1), shade_rgb(s, 2) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}
