#ifndef SHA1_H
#define SHA1_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[5];
    uint64_t bitlen;
    uint8_t  buf[64];
    uint32_t buflen;
} sha1_ctx_t;

void sha1_init(sha1_ctx_t *ctx);
void sha1_update(sha1_ctx_t *ctx, const void *data, size_t len);
void sha1_final(sha1_ctx_t *ctx, uint8_t out[20]);
void sha1(const void *data, size_t len, uint8_t out[20]);

void hmac_sha1(const uint8_t *key, size_t key_len,
               const uint8_t *data, size_t data_len,
               uint8_t out[20]);

#endif
