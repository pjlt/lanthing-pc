/* Implementation of SHA1 hash function.
 * Original author:  Steve Reid <sreid@sea-to-sky.net>
 * Contributions by: James H. Brown <jbrown@burgoyne.com>, Saul Kravitz
 * <Saul.Kravitz@celera.com>, and Ralph Giles <giles@ghostscript.com>
 * Modified by WaterJuice retaining Public Domain license.
 *
 * This is free and unencumbered software released into the public domain
 * June 2013 waterjuice.org
 */

#include <memory.h>
#include <modules/p2p/stuns/sha1.h>

typedef union {
    uint8_t c[64];
    uint32_t l[16];
} CHAR64LONG16;

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifdef WORDS_BIGENDIAN
#define blk0(i) block->l[i]
#else
#define blk0(i)                                                                                    \
    (block->l[i] = (rol(block->l[i], 24) & 0xFF00FF00) | (rol(block->l[i], 8) & 0x00FF00FF))
#endif
#define blk(i)                                                                                     \
    (block->l[i & 15] = rol(block->l[(i + 13) & 15] ^ block->l[(i + 8) & 15] ^                     \
                                block->l[(i + 2) & 15] ^ block->l[i & 15],                         \
                            1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v, w, x, y, z, i)                                                                       \
    z += ((w & (x ^ y)) ^ y) + blk0(i) + 0x5A827999ul + rol(v, 5);                                 \
    w = rol(w, 30);
#define R1(v, w, x, y, z, i)                                                                       \
    z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5A827999ul + rol(v, 5);                                  \
    w = rol(w, 30);
#define R2(v, w, x, y, z, i)                                                                       \
    z += (w ^ x ^ y) + blk(i) + 0x6ED9EBA1ul + rol(v, 5);                                          \
    w = rol(w, 30);
#define R3(v, w, x, y, z, i)                                                                       \
    z += (((w | x) & y) | (w & x)) + blk(i) + 0x8F1BBCDCul + rol(v, 5);                            \
    w = rol(w, 30);
#define R4(v, w, x, y, z, i)                                                                       \
    z += (w ^ x ^ y) + blk(i) + 0xCA62C1D6ul + rol(v, 5);                                          \
    w = rol(w, 30);

/* Hash a single 512-bit block. This is the core of the algorithm. */
static void SHA1Transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e;
    uint8_t workspace[64];
    CHAR64LONG16* block = (CHAR64LONG16*)workspace;

    memcpy(block, buffer, 64);

    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a, b, c, d, e, 0);
    R0(e, a, b, c, d, 1);
    R0(d, e, a, b, c, 2);
    R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);
    R0(a, b, c, d, e, 5);
    R0(e, a, b, c, d, 6);
    R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);
    R0(b, c, d, e, a, 9);
    R0(a, b, c, d, e, 10);
    R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12);
    R0(c, d, e, a, b, 13);
    R0(b, c, d, e, a, 14);
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);

    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

/* SHA1Init - Initialize new context */
void rtc2_SHA1_Init(SHA1_CTX* context) {
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301ul;
    context->state[1] = 0xEFCDAB89ul;
    context->state[2] = 0x98BADCFEul;
    context->state[3] = 0x10325476ul;
    context->state[4] = 0xC3D2E1F0ul;
    context->count[0] = context->count[1] = 0;
}

/* Run your data through this. */
void rtc2_SHA1_Update(SHA1_CTX* context, const uint8_t* data, size_t len) {
    size_t i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += (uint32_t)len << 3) < (len << 3))
        context->count[1]++;
    context->count[1] += ((uint32_t)len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64)
            SHA1Transform(context->state, &data[i]);
        j = 0;
    }
    else
        i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}

/* Add padding and return the message digest. */
void rtc2_SHA1_Final(uint8_t digest[20], SHA1_CTX* context) {
    size_t i;
    uint8_t finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) &
                                  255); /* Endian independent */
    }
    rtc2_SHA1_Update(context, (uint8_t*)"\x80", 1);
    while ((context->count[0] & 504) != 448)
        rtc2_SHA1_Update(context, (uint8_t*)"\0", 1);
    rtc2_SHA1_Update(context, finalcount, 8); /* Should cause a SHA1Transform() */
    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}
