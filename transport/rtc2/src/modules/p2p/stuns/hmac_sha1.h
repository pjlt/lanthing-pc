/*
 * Functions to implement RFC-2104 (HMAC with SHA-1 hashes).
 * Placed into the public domain.
 */

#ifndef __HMAC_SHA1_H__
#define __HMAC_SHA1_H__

#include <modules/p2p/stuns/sha1.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _HMAC_SHA1_CTX {
    SHA1_CTX sha1ctx;
    uint8_t k_ipad[65];
    uint8_t k_opad[65];
} HMAC_SHA1_CTX;

void HMAC_SHA1_Init(HMAC_SHA1_CTX* ctx, const uint8_t* key, size_t key_len);
void HMAC_SHA1_Update(HMAC_SHA1_CTX* ctx, const uint8_t* data, size_t data_len);
void HMAC_SHA1_Final(uint8_t digest[20], HMAC_SHA1_CTX* ctx);

#ifdef __cplusplus
};
#endif

#endif /* __HMAC_SHA1_H__ */
