#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <tiffio.h>

#if 0
#include <png.h>

static void png_write_data(png_structp png_ptr, 
            png_bytep data, png_size_t length)
{
	FILE *fh = (FILE *) png_get_io_ptr(png_ptr);
	if (fh)
        fwrite(data, 1, length, fh);
}

static void write_rgb_png(const char *fn, 
            const int w, const int h,
            const uint16_t *dst)
{
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_16 **row_pointers = NULL;
    FILE *fh = fopen(fn, "wb");;
	int ok = 1;

	row_pointers = (png_uint_16 **) malloc(h * sizeof(png_uint_16*));
	if (!row_pointers)
		ok = 0;
	
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		ok = 0;
	} else {
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
			ok = 0;
	}

	png_set_write_fn(png_ptr, fh, png_write_data, NULL);

	if (setjmp(png_jmpbuf(png_ptr))) {
		ok = 0;
	}
	
    png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
    //png_set_compression_level(png_ptr, 1);
    //png_set_swap(png_ptr);

	if (ok) {
        const uint16_t *p = dst;

		png_set_IHDR(png_ptr, info_ptr,
			w, h, 16, PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE, 
            PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
		
		png_write_info(png_ptr, info_ptr);

		for (int y = 0; y < h; ++y) {
			row_pointers[y] = (png_uint_16 *) p;
            p += w * 3;
        }
		
		png_write_image(png_ptr, (png_byte **)row_pointers);
		
		png_write_end(png_ptr, NULL);
	}

	if (png_ptr)
		png_destroy_write_struct(&png_ptr, &info_ptr);
	if (row_pointers)
		free(row_pointers);
    if (fh)
        fclose(fh);
}
#endif

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

#if 0
static void write_file(const char *fn, const uint8_t *buf, const int len)
{
    FILE *fh;

    fh = fopen(fn, "wb");
    assert(fh != NULL);
    fwrite(buf, 1, len, fh);
    fclose(fh);
}
#endif

#if 0
static void write_rgb(const char *fn, 
            const int w, const int h,
            const uint16_t *r, const uint16_t *g, const uint16_t *b)
{
    FILE *fh;

    fh = fopen(fn, "wb");
    assert(fh != NULL);

    for (int i = 0; i < (w * h); ++i) {
        fwrite(r++, 1, sizeof(uint16_t), fh);
        fwrite(g++, 1, sizeof(uint16_t), fh);
        fwrite(b++, 1, sizeof(uint16_t), fh);
    }
    fclose(fh);
}

typedef struct _raw_grp_t {
    uint16_t r;
    uint16_t gr;
    uint16_t gb;
    uint16_t b;
} raw_grp_t;

static inline uint16_t rv10(uint16_t v)
{
    return  ((v & 0x001) << 9)
        |   ((v & 0x002) << 7)
        |   ((v & 0x004) << 5)
        |   ((v & 0x008) << 3)
        |   ((v & 0x010) << 1)
        |   ((v & 0x020) >> 1)
        |   ((v & 0x040) >> 3)
        |   ((v & 0x080) >> 5)
        |   ((v & 0x100) >> 7)
        |   ((v & 0x200) >> 9); 
}
#endif

static uint16_t *expand_10_to_16(const uint8_t *buf, const int len)
{
    const uint8_t *ptr = buf;
    const int filelen = (len * 8) / 10;
    uint16_t *image = (uint16_t *) malloc(filelen * sizeof(uint16_t));
    uint16_t *result = image;
    double sv[8] = {    0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0  };
    double count = 0.0;
    int phase = 0;

    assert(image != NULL);

    while (ptr < (buf+len)) {
        uint16_t v0, v1, v2, v3;
        
        v0 = (*(ptr+0) << 2) | ((*(ptr+4) & 0x03) >> 0);
        v1 = (*(ptr+1) << 2) | ((*(ptr+4) & 0x0C) >> 2);
        v2 = (*(ptr+2) << 2) | ((*(ptr+4) & 0x30) >> 4);
        v3 = (*(ptr+3) << 2) | ((*(ptr+4) & 0xC0) >> 6);

        #if 0
        fprintf (stderr, "%02x %02x %02x %02x %02x -> %03x %03x %03x %03x\n",
            *(ptr + 0),
            *(ptr + 1),
            *(ptr + 2),
            *(ptr + 3),
            *(ptr + 4),
            v0, v1, v2, v3);
        #endif

        ptr += 5;
        
        *(image++) = v0;
        *(image++) = v1;
        *(image++) = v2;
        *(image++) = v3;

        sv[phase*4 + 0] += v0;
        sv[phase*4 + 1] += v1;
        sv[phase*4 + 2] += v2;
        sv[phase*4 + 3] += v3;
        count += 1.0;

        phase = (phase + 1) & 0x1;
    }

    for (int i = 0; i < 8; ++i) {
        fprintf (stderr, "sv[%d] = %.1f\n", i, sv[i] / (count / 8.0));
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
            #if 0
            uint16_t pp = get_pixel(buf, w, h, col, row) << 6;
            *(ptr++) = pp;
            *(ptr++) = pp;
            *(ptr++) = pp;
            #endif
            
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
            //fprintf (stderr, "%d,%d = %d,%d,%d\n", col, row, r, g, b);
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
