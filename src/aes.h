#ifndef AES_H
#define AES_H
#include <stdint.h>
#include <stddef.h>

#define AES_BLOCK_SIZE 16

typedef struct {
    uint32_t round_key[44]; /* AES-128: 11 round keys x 4 words */
} aes128_ctx_t;

void aes128_init(aes128_ctx_t *ctx, const uint8_t key[16]);
void aes128_encrypt_block(const aes128_ctx_t *ctx, const uint8_t in[16], uint8_t out[16]);
void aes128_decrypt_block(const aes128_ctx_t *ctx, const uint8_t in[16], uint8_t out[16]);

/* CBC helpers: len must be a multiple of 16. iv is consumed (not mutated). */
void aes128_cbc_encrypt(const aes128_ctx_t *ctx, const uint8_t iv[16],
                         const uint8_t *in, uint8_t *out, size_t len);
void aes128_cbc_decrypt(const aes128_ctx_t *ctx, const uint8_t iv[16],
                         const uint8_t *in, uint8_t *out, size_t len);

#endif
