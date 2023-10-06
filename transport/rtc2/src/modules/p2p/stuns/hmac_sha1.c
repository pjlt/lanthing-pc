/*
 * Functions to implement RFC-2104 (HMAC with SHA-1 hashes).
 * Placed into the public domain.
 */

#include <memory.h>
#include <modules/p2p/stuns/hmac_sha1.h>

/*
 * Encode a string using HMAC - see RFC-2104 for details.
 */
void HMAC_SHA1_Init(HMAC_SHA1_CTX* ctx, const uint8_t* key, size_t key_len) {
    uint8_t tk[20];
    int i;

    /* if key is longer than 64 bytes reset it to key=SHA1(key) */
    if (key_len > 64) {
        SHA1_CTX tctx;

        rtc2_SHA1_Init(&tctx);
        rtc2_SHA1_Update(&tctx, key, key_len);
        rtc2_SHA1_Final(tk, &tctx);

        key = tk;
        key_len = 20;
    }

    /*
     * the HMAC_SHA1 transform looks like:
     *
     * SHA1(K XOR opad, SHA1(K XOR ipad, text))
     *
     * where K is an n byte key
     * ipad is the byte 0x36 repeated 64 times
     * opad is the byte 0x5c repeated 64 times
     * and text is the data being protected
     */

    /* start out by storing key in pads */
    memset(ctx->k_ipad, 0, sizeof(ctx->k_ipad));
    memset(ctx->k_opad, 0, sizeof(ctx->k_opad));
    memcpy(ctx->k_ipad, key, key_len);
    memcpy(ctx->k_opad, key, key_len);

    /* XOR key with ipad and opad values */
    for (i = 0; i < 64; i++) {
        ctx->k_ipad[i] ^= 0x36;
        ctx->k_opad[i] ^= 0x5c;
    }

    rtc2_SHA1_Init(&ctx->sha1ctx);                    /* init context for 1st pass */
    rtc2_SHA1_Update(&ctx->sha1ctx, ctx->k_ipad, 64); /* start with inner pad */
}

void HMAC_SHA1_Update(HMAC_SHA1_CTX* ctx, const uint8_t* data, size_t data_len) {
    rtc2_SHA1_Update(&ctx->sha1ctx, data, data_len); /* then text of datagram */
}

void HMAC_SHA1_Final(uint8_t digest[20], HMAC_SHA1_CTX* ctx) {
    rtc2_SHA1_Final(digest, &ctx->sha1ctx); /* finish up 1st pass */

    /* perform outer SHA1 */
    rtc2_SHA1_Init(&ctx->sha1ctx);                    /* init context for 2nd pass */
    rtc2_SHA1_Update(&ctx->sha1ctx, ctx->k_opad, 64); /* start with outer pad */
    rtc2_SHA1_Update(&ctx->sha1ctx, digest, 20);      /* then results of 1st hash */
    rtc2_SHA1_Final(digest, &ctx->sha1ctx);           /* finish up 2nd pass */
}
