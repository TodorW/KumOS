#ifndef BIGNUM_H
#define BIGNUM_H
#include <stdint.h>
#include <stddef.h>

/* Fixed-width 4096-bit big integers (covers RSA-2048/3072/4096 moduli).
   Everything is little-endian limbs, always zero-padded to full width -
   simpler to get right than variable-length arithmetic, and this only
   ever runs once per TLS handshake so the constant-factor cost doesn't
   matter. */
#define BN_LIMBS 128

typedef struct { uint32_t d[BN_LIMBS]; } bignum_t;
typedef struct { uint32_t d[2*BN_LIMBS]; } bignum_wide_t;

void bn_zero(bignum_t *a);
void bn_from_u32(bignum_t *a, uint32_t v);
void bn_from_be_bytes(bignum_t *a, const uint8_t *bytes, int len);
int  bn_to_be_bytes(const bignum_t *a, uint8_t *out, int out_len);

int  bn_cmp(const bignum_t *a, const bignum_t *b);
int  bn_is_zero(const bignum_t *a);
int  bn_byte_len(const bignum_t *a);

void bn_mul(const bignum_t *a, const bignum_t *b, bignum_wide_t *r);
void bn_mod_wide(const bignum_wide_t *val, const bignum_t *mod, bignum_t *rem);
void bn_modmul(const bignum_t *a, const bignum_t *b, const bignum_t *mod, bignum_t *out);

/* RSA public-exponent modexp: exponent is always small (65537, or 3),
   never a full bignum, for the encrypt-with-public-key operation we need. */
void bn_modexp_u32(const bignum_t *base, uint32_t exp, const bignum_t *mod, bignum_t *result);

#endif
