/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 */

#ifndef __SHA1_H__
#define __SHA1_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _SHA1_CTX {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

void rtc2_SHA1_Init(SHA1_CTX* context);
void rtc2_SHA1_Update(SHA1_CTX* context, const uint8_t* data, size_t len);
void rtc2_SHA1_Final(uint8_t digest[20], SHA1_CTX* context);

#ifdef __cplusplus
};
#endif

#endif /* __SHA1_H__ */