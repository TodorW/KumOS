#ifndef X509_H
#define X509_H
#include <stdint.h>
#include "bignum.h"

/* Pulls the RSA public key (modulus + exponent) straight out of a DER
   certificate's subjectPublicKeyInfo. Everything else in the certificate
   (issuer, subject, validity, signature) is walked past as opaque TLV
   blobs and never interpreted - there is no chain-of-trust validation,
   no signature check, no expiry/hostname check. This gets you real
   encryption against whoever answered the TCP connection, not real
   authentication. Treat it the way you'd treat curl -k. */
int x509_extract_rsa_pubkey(const uint8_t *cert_der, uint32_t cert_len,
                             bignum_t *modulus, uint32_t *exponent);

#endif
