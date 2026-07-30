#include "sha1.h"

static inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32-n)); }

static void sha1_transform(sha1_ctx_t *ctx, const uint8_t *data) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)data[i*4]<<24) | ((uint32_t)data[i*4+1]<<16) |
               ((uint32_t)data[i*4+2]<<8) | (uint32_t)data[i*4+3];
    for (int i = 16; i < 80; i++)
        w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    uint32_t a=ctx->state[0], b=ctx->state[1], c=ctx->state[2];
    uint32_t d=ctx->state[3], e=ctx->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | ((~b) & d);      k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                 k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                 k = 0xCA62C1D6; }

        uint32_t temp = rotl(a,5) + f + e + k + w[i];
        e = d; d = c; c = rotl(b,30); b = a; a = temp;
    }

    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c;
    ctx->state[3]+=d; ctx->state[4]+=e;
}

void sha1_init(sha1_ctx_t *ctx) {
    ctx->state[0]=0x67452301; ctx->state[1]=0xEFCDAB89; ctx->state[2]=0x98BADCFE;
    ctx->state[3]=0x10325476; ctx->state[4]=0xC3D2E1F0;
    ctx->bitlen = 0;
    ctx->buflen = 0;
}

void sha1_update(sha1_ctx_t *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t*)data;
    ctx->bitlen += (uint64_t)len * 8;
    while (len > 0) {
        uint32_t take = 64 - ctx->buflen;
        if (take > len) take = (uint32_t)len;
        for (uint32_t i = 0; i < take; i++) ctx->buf[ctx->buflen+i] = p[i];
        ctx->buflen += take;
        p += take;
        len -= take;
        if (ctx->buflen == 64) {
            sha1_transform(ctx, ctx->buf);
            ctx->buflen = 0;
        }
    }
}

void sha1_final(sha1_ctx_t *ctx, uint8_t out[20]) {
    uint64_t bitlen = ctx->bitlen;
    uint8_t pad = 0x80;
    sha1_update(ctx, &pad, 1);

    uint8_t zero = 0;
    while (ctx->buflen != 56) sha1_update(ctx, &zero, 1);

    uint8_t lenbytes[8];
    for (int i = 0; i < 8; i++) lenbytes[i] = (uint8_t)(bitlen >> (56 - 8*i));
    for (int i = 0; i < 8; i++) ctx->buf[ctx->buflen+i] = lenbytes[i];
    ctx->buflen += 8;
    sha1_transform(ctx, ctx->buf);
    ctx->buflen = 0;

    for (int i = 0; i < 5; i++) {
        out[i*4+0] = (uint8_t)(ctx->state[i] >> 24);
        out[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        out[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        out[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

void sha1(const void *data, size_t len, uint8_t out[20]) {
    sha1_ctx_t ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, out);
}

void hmac_sha1(const uint8_t *key, size_t key_len,
               const uint8_t *data, size_t data_len,
               uint8_t out[20]) {
    uint8_t k[64];
    uint8_t k_ipad[64], k_opad[64];

    if (key_len > 64) {
        sha1(key, key_len, k);
        for (size_t i = 20; i < 64; i++) k[i] = 0;
    } else {
        size_t i;
        for (i = 0; i < key_len; i++) k[i] = key[i];
        for (; i < 64; i++) k[i] = 0;
    }

    for (int i = 0; i < 64; i++) {
        k_ipad[i] = (uint8_t)(k[i] ^ 0x36);
        k_opad[i] = (uint8_t)(k[i] ^ 0x5c);
    }

    sha1_ctx_t ctx;
    uint8_t inner[20];
    sha1_init(&ctx);
    sha1_update(&ctx, k_ipad, 64);
    sha1_update(&ctx, data, data_len);
    sha1_final(&ctx, inner);

    sha1_init(&ctx);
    sha1_update(&ctx, k_opad, 64);
    sha1_update(&ctx, inner, 20);
    sha1_final(&ctx, out);
}
