#include "astc_codec_internals.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef JSASTC_EMSCRIPTEN
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYEXR_IMPLEMENTATION
#endif

#include "stb_image.h"
#include "stb_image_write.h"
#include "tinyexr.h"

#define MAGIC_FILE_CONSTANT 0x5CA1AB13

struct astc_header
{
    uint8_t magic[4];
    uint8_t blockdim_x;
    uint8_t blockdim_y;
    uint8_t blockdim_z;
    uint8_t xsize[3]; // x-size = xsize[0] + xsize[1] + xsize[2]
    uint8_t ysize[3]; // x-size, y-size and z-size are given in texels;
    uint8_t zsize[3]; // block count is inferred
};

astc_codec_image * load_astc_buffer(uint8_t * inputBuffer, size_t inputBufferSize, int bitness, astc_decode_mode decode_mode, swizzlepattern swz_decode)
{
    size_t headerSize = sizeof(astc_header);
    if (inputBufferSize < headerSize) {
        return NULL;
    }

    int x, y, z;
    astc_header hdr;
    memcpy(&hdr, inputBuffer, headerSize);
    inputBuffer += headerSize;
    inputBufferSize -= headerSize;

    uint32_t magicval = (uint32_t)(hdr.magic[0]) + (uint32_t)(hdr.magic[1]) * 256 + (uint32_t)(hdr.magic[2]) * 65536 +
                        (uint32_t)(hdr.magic[3]) * 16777216;

    if (magicval != MAGIC_FILE_CONSTANT) {
        return NULL;
    }

    int xdim = hdr.blockdim_x;
    int ydim = hdr.blockdim_y;
    int zdim = hdr.blockdim_z;

    if ((xdim < 3 || xdim > 6 || ydim < 3 || ydim > 6 || zdim < 3 || zdim > 6) &&
        (xdim < 4 || xdim == 7 || xdim == 9 || xdim == 11 || xdim > 12 || ydim < 4 || ydim == 7 || ydim == 9 || ydim == 11 ||
         ydim > 12 || zdim != 1)) {
        printf("File not recognized %d %d %d\n", xdim, ydim, zdim);
        return NULL;
    }

    int xsize = hdr.xsize[0] + 256 * hdr.xsize[1] + 65536 * hdr.xsize[2];
    int ysize = hdr.ysize[0] + 256 * hdr.ysize[1] + 65536 * hdr.ysize[2];
    int zsize = hdr.zsize[0] + 256 * hdr.zsize[1] + 65536 * hdr.zsize[2];

    if (xsize == 0 || ysize == 0 || zsize == 0) {
        printf("File has zero dimension %d %d %d\n", xsize, ysize, zsize);
        return NULL;
    }

    int xblocks = (xsize + xdim - 1) / xdim;
    int yblocks = (ysize + ydim - 1) / ydim;
    int zblocks = (zsize + zdim - 1) / zdim;

    size_t bytes_to_read = xblocks * yblocks * zblocks * 16;
    if (inputBufferSize < bytes_to_read) {
        printf("Truncated file\n");
        return NULL;
    }
    uint8_t * buffer = inputBuffer;

    astc_codec_image * img = alloc_image(bitness, xsize, ysize, zsize, 0);
    img->linearize_srgb = 0;
    img->rgb_force_use_of_hdr = 0;
    img->alpha_force_use_of_hdr = 0;

    block_size_descriptor * bsd = (block_size_descriptor *)malloc(sizeof(block_size_descriptor));
    init_block_size_descriptor(xdim, ydim, zdim, bsd);

    imageblock pb;
    for (z = 0; z < zblocks; z++) {
        for (y = 0; y < yblocks; y++) {
            for (x = 0; x < xblocks; x++) {
                int offset = (((z * yblocks + y) * xblocks) + x) * 16;
                uint8_t * bp = buffer + offset;
                physical_compressed_block pcb = *(physical_compressed_block *)bp;
                symbolic_compressed_block scb;
                physical_to_symbolic(bsd, pcb, &scb);
                decompress_symbolic_block(img, decode_mode, bsd, x * xdim, y * ydim, z * zdim, &scb, &pb);
                write_imageblock(img, &pb, bsd, x * xdim, y * ydim, z * zdim, swz_decode);
            }
        }
    }

    term_block_size_descriptor(bsd);
    free(bsd);
    return img;
}

// From https://vorbrodt.blog/2019/03/23/base64-encoding/
namespace base64
{
static const char kEncodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char kPadCharacter = '=';

using byte = std::uint8_t;

inline std::string encode(const std::vector<byte> & input)
{
    std::string encoded;
    encoded.reserve(((input.size() / 3) + (input.size() % 3 > 0)) * 4);

    std::uint32_t temp {};
    auto it = input.begin();

    for (std::size_t i = 0; i < input.size() / 3; ++i) {
        temp = (*it++) << 16;
        temp += (*it++) << 8;
        temp += (*it++);
        encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
        encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
        encoded.append(1, kEncodeLookup[(temp & 0x00000FC0) >> 6]);
        encoded.append(1, kEncodeLookup[(temp & 0x0000003F)]);
    }

    switch (input.size() % 3) {
        case 1:
            temp = (*it++) << 16;
            encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
            encoded.append(2, kPadCharacter);
            break;
        case 2:
            temp = (*it++) << 16;
            temp += (*it++) << 8;
            encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
            encoded.append(1, kEncodeLookup[(temp & 0x00000FC0) >> 6]);
            encoded.append(1, kPadCharacter);
            break;
    }

    return encoded;
}

std::vector<byte> decode(const std::string & input)
{
    if (input.length() % 4)
        return std::vector<base64::byte>();

    std::size_t padding {};

    if (input.length()) {
        if (input[input.length() - 1] == kPadCharacter)
            padding++;
        if (input[input.length() - 2] == kPadCharacter)
            padding++;
    }

    std::vector<byte> decoded;
    decoded.reserve(((input.length() / 4) * 3) - padding);

    std::uint32_t temp {};
    auto it = input.begin();

    while (it < input.end()) {
        for (std::size_t i = 0; i < 4; ++i) {
            temp <<= 6;
            if (*it >= 0x41 && *it <= 0x5A)
                temp |= *it - 0x41;
            else if (*it >= 0x61 && *it <= 0x7A)
                temp |= *it - 0x47;
            else if (*it >= 0x30 && *it <= 0x39)
                temp |= *it + 0x04;
            else if (*it == 0x2B)
                temp |= 0x3E;
            else if (*it == 0x2F)
                temp |= 0x3F;
            else if (*it == kPadCharacter) {
                switch (input.end() - it) {
                    case 1:
                        decoded.push_back((temp >> 16) & 0x000000FF);
                        decoded.push_back((temp >> 8) & 0x000000FF);
                        return decoded;
                    case 2:
                        decoded.push_back((temp >> 10) & 0x000000FF);
                        return decoded;
                    default:
                        return std::vector<base64::byte>();
                }
            } else
                return std::vector<base64::byte>();

            ++it;
        }

        decoded.push_back((temp >> 16) & 0x000000FF);
        decoded.push_back((temp >> 8) & 0x000000FF);
        decoded.push_back((temp)&0x000000FF);
    }

    return decoded;
}
} // namespace base64

struct STBIContext
{
    std::vector<uint8_t> data;
};

static void writeToVector(void * context, void * data, int size)
{
    STBIContext * sc = (STBIContext *)context;
    uint8_t * data8 = (uint8_t *)data;
    sc->data.insert(sc->data.end(), data8, data8 + size);
}

std::string convertToPNG(const std::string & input)
{
    // initialization routines
    prepare_angular_tables();
    build_quantization_mode_table();

    std::vector<uint8_t> astcBuffer = base64::decode(input);
    if (astcBuffer.size() == 0) {
        return "";
    }

    swizzlepattern swizzleRGBA = { 0, 1, 2, 3 };
    uint8_t * inputBuffer = &astcBuffer[0];
    size_t inputBufferSize = astcBuffer.size();
    astc_codec_image * astc = load_astc_buffer(&astcBuffer[0], inputBufferSize, 8, DECODE_LDR_SRGB, swizzleRGBA);
    if (!astc) {
        return "";
    }

    // printf("Got ASTC: %d x %d\n", astc->xsize, astc->ysize);

    STBIContext context;
    uint8_t * buf = unorm8x4_array_from_astc_img(astc, 1);
    int res = stbi_write_png_to_func(writeToVector, &context, astc->xsize, astc->ysize, 4, buf, astc->xsize * 4);
    free(buf);
    free_image(astc);

#ifndef JSASTC_EMSCRIPTEN
    FILE * f = fopen("debug.png", "wb");
    fwrite(&context.data[0], 1, context.data.size(), f);
    fclose(f);
#endif

    return std::string("data:image/png;base64,") + base64::encode(context.data);
}

#ifdef JSASTC_EMSCRIPTEN
#include <emscripten/bind.h>
using namespace emscripten;

EMSCRIPTEN_BINDINGS(jsastc)
{
    function("convertToPNG", &convertToPNG);
}
#endif
