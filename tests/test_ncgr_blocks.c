/* Regression checks for parsed NCGR data with sentinel dimensions. */

#include <string.h>

#include "g2d.h"
#include "port_test.h"

#define NCGR_HEADER_SIZE 16u
#define CHAR_HEADER_SIZE 24u
#define PAYLOAD_SIZE 128u
#define TEST_FILE_SIZE (NCGR_HEADER_SIZE + 8u + CHAR_HEADER_SIZE + PAYLOAD_SIZE)

static void put16(u8 *dst, u16 value)
{
    dst[0] = (u8)value;
    dst[1] = (u8)(value >> 8);
}

static void put32(u8 *dst, u32 value)
{
    dst[0] = (u8)value;
    dst[1] = (u8)(value >> 8);
    dst[2] = (u8)(value >> 16);
    dst[3] = (u8)(value >> 24);
}

static void make_ncgr(u8 file[TEST_FILE_SIZE], u32 pixel_fmt,
                      const u8 payload[PAYLOAD_SIZE])
{
    u8 *body = file + NCGR_HEADER_SIZE + 8u;

    memset(file, 0, TEST_FILE_SIZE);
    memcpy(file, "RGCN", 4);
    put16(file + 12, NCGR_HEADER_SIZE);
    memcpy(file + NCGR_HEADER_SIZE, "RAHC", 4);
    put32(file + NCGR_HEADER_SIZE + 4u, 8u + CHAR_HEADER_SIZE + PAYLOAD_SIZE);

    put16(body + 0, G2D_NCGR_DIM_SENTINEL);
    put16(body + 2, G2D_NCGR_DIM_SENTINEL);
    put32(body + 4, pixel_fmt);
    put32(body + 8, 0);
    put32(body + 12, 0);
    put32(body + 16, PAYLOAD_SIZE);
    put32(body + 20, CHAR_HEADER_SIZE);
    memcpy(body + CHAR_HEADER_SIZE, payload, PAYLOAD_SIZE);
}

int main(int argc, char **argv)
{
    G2dNcgr ncgr;
    u8 file[TEST_FILE_SIZE], packed4[PAYLOAD_SIZE], pixels8[PAYLOAD_SIZE];
    u8 expected4[64], out[64], zero[64] = { 0 };
    u32 i;

    BW_TEST_SETUP(argc, argv);
    for (i = 0; i < PAYLOAD_SIZE; i++) {
        packed4[i] = (u8)(((i & 0x0Fu) << 4) | ((15u - i) & 0x0Fu));
        pixels8[i] = (u8)(i ^ 0xA5u);
    }

    make_ncgr(file, G2D_NCGR_FMT_4BPP, packed4);
    BW_CHECK_EQ("sentinel 4bpp NCGR parses", g2d_ncgr_parse(&ncgr, file, sizeof file), 0);
    BW_CHECK_EQ("sentinel 4bpp dimensions retained", ncgr.sentinel_dims, 1);
    BW_CHECK_EQ("sentinel NCGR avoids giant pixel canvas", ncgr.pixels == NULL, 1);
    for (i = 0; i < 64; i++) {
        expected4[i] = (i & 1u) ? (u8)(packed4[i / 2u] >> 4)
                                : (u8)(packed4[i / 2u] & 0x0Fu);
    }
    g2d_ncgr_block(&ncgr, 0, out);
    BW_CHECK_EQ("sentinel 4bpp low-nibble-first decode",
                memcmp(out, expected4, sizeof expected4), 0);
    g2d_ncgr_block(&ncgr, 3, out);
    for (i = 0; i < 64; i++) {
        expected4[i] = (i & 1u) ? (u8)(packed4[96u + i / 2u] >> 4)
                                : (u8)(packed4[96u + i / 2u] & 0x0Fu);
    }
    BW_CHECK_EQ("sentinel 4bpp tile offset", memcmp(out, expected4, sizeof expected4), 0);
    g2d_ncgr_free(&ncgr);

    make_ncgr(file, G2D_NCGR_FMT_8BPP, pixels8);
    BW_CHECK_EQ("sentinel 8bpp NCGR parses", g2d_ncgr_parse(&ncgr, file, sizeof file), 0);
    g2d_ncgr_block(&ncgr, 1, out);
    BW_CHECK_EQ("sentinel 8bpp tile offset", memcmp(out, pixels8 + 64, sizeof out), 0);
    g2d_ncgr_block(&ncgr, 2, out);
    BW_CHECK_EQ("sentinel tile past payload is transparent", memcmp(out, zero, sizeof out), 0);
    ncgr.tile_data_size = 127;
    g2d_ncgr_block(&ncgr, 1, out);
    BW_CHECK_EQ("partial trailing tile is transparent", memcmp(out, zero, sizeof out), 0);
    g2d_ncgr_free(&ncgr);

    BW_REPORT("test_ncgr_blocks");
}
