#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <tiffio.h>

static void write_rgb_tiff(const char *fn, 
            const int w, const int h,
            const uint16_t *dst)
{
    const uint16_t  *p = dst;
    TIFF            *out;
    int             rps = w * h * 3 * 2;

    out = TIFFOpen(fn, "w");
    assert(out != NULL);

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    // TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    
    rps = TIFFDefaultStripSize(out, rps);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rps);
    
    for (int y = 0; y < h; ++y) {
        TIFFWriteScanline(out, (unsigned char *)p, y, 0);
        p += w * 3; 
    }

    TIFFClose(out);
}

static uint8_t *read_file(const char *fn, int *len)
{
    uint8_t *buf = NULL;
    FILE *fh;
    int ret;

    assert(len != NULL);

    fh = fopen(fn, "rb");
    assert(fh != NULL);
    fseek(fh, 0, SEEK_END);
    *len = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    buf = (uint8_t *) malloc(*len);
    ret = fread(buf, 1, *len, fh);
    assert(ret == *len);
    fclose(fh);

    return buf;
}

static uint16_t *expand_10_to_16(const uint8_t *buf, const int len)
{
    const uint8_t *ptr = buf;
    const int filelen = (len * 8) / 10;
    uint16_t *image = (uint16_t *) malloc(filelen * sizeof(uint16_t));
    uint16_t *result = image;

    assert(image != NULL);

    while (ptr < (buf+len)) {
        *(image++) = (*(ptr+0) << 2) | ((*(ptr+4) & 0x03) >> 0);
        *(image++) = (*(ptr+1) << 2) | ((*(ptr+4) & 0x0C) >> 2);
        *(image++) = (*(ptr+2) << 2) | ((*(ptr+4) & 0x30) >> 4);
        *(image++) = (*(ptr+3) << 2) | ((*(ptr+4) & 0xC0) >> 6);

        ptr += 5;
    }

    return result;
}

static uint16_t *allocate_image(const int w, const int h)
{
    uint16_t *p = (uint16_t *) malloc(sizeof(uint16_t) * w * h * 3);
    assert(p != NULL);
    memset(p, 0, sizeof(uint16_t) * w * h * 3);
    return p;
}

static inline uint16_t get_pixel(const uint16_t *data,
                const int w, const int h,
                const int x, const int y)
{
    return *(data + (y * w) + x);
}

static void compute_rgb(const int w, const int h,
                const uint16_t *buf,
                uint16_t *ptr)
{
    const int off_r[16][2]   = { 
            {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 },
            { -1,  0 }, { -1,  0 }, {  1,  0 }, {  1,  0 },
            {  0, -1 }, {  0, -1 }, {  0,  1 }, {  0,  1 },
            { -1, -1 }, { -1,  1 }, {  1, -1 }, {  1,  1 }
    };
    const int off_g[16][2]   = { 
            { -1,  0 }, {  0, -1 }, {  1,  0 }, {  0,  1 },
            {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 },
            {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 },
            { -1,  0 }, {  0, -1 }, {  1,  0 }, {  0,  1 }
    };
    const int off_b[16][2]   = { 
            { -1, -1 }, { -1,  1 }, {  1, -1 }, {  1,  1 },
            {  0, -1 }, {  0, -1 }, {  0,  1 }, {  0,  1 },
            { -1,  0 }, { -1,  0 }, {  1,  0 }, {  1,  0 },
            {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }
    };

    ptr += w * 3;

    int py = 1;
    for (int row = 1; row < (h - 1); row += 1) {
        int px = 0;
        
        ptr += 3;
        for (int col = 1; col < (w - 1); col += 1) {
            const int pp = ((py << 1) + px) << 2;
            uint16_t r = 0, g = 0, b = 0;
            for (int i = 0; i < 4; ++i) {
                const int p = pp + i;
                r += get_pixel(buf, w, h, 
                            col + off_r[p][0],
                            row + off_r[p][1]);
                g += get_pixel(buf, w, h, 
                            col + off_g[p][0],
                            row + off_g[p][1]);
                b += get_pixel(buf, w, h,
                            col + off_b[p][0],
                            row + off_b[p][1]);
            }
            *(ptr++) = r << 4;
            *(ptr++) = g << 4;
            *(ptr++) = b << 4;
            px = (px + 1) & 0x1;
        }

        ptr += 3;
        py = (py + 1) & 0x1;
    }
}

int main(int argc, char *argv[])
{
    const int width = 7728;
    const int height = 5368;

    if (argc >= 3) {
        char *in_f = argv[1], *out_f = argv[2];
        int len = 0;
        uint8_t *src = read_file(in_f, &len);
        uint16_t *raw = expand_10_to_16(src, len);
        uint16_t *dst = allocate_image(width, height);
        compute_rgb(width, height, raw, dst);
        write_rgb_tiff(out_f, width, height, dst);
    } else {
        fprintf (stderr, "%s <in> <out>\n", argv[0]);
    }

    return 0;
}
