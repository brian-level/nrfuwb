/*
 * Copyright (c) 2022, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_secpk1_ecc.h"

#if CRYPTO_SECPK1_ECC

#define COMPONENT_NAME secpk1_ecc
#include "Logging.h"
#include "AssertMacros.h"
#include "Crypto.h"

#include "secp256k1.h"
#include "secp256k1_ecdh.h"

#include <mbedtls/sha256.h>

#include <string.h>
#include <stdbool.h>

static int ecdh_hash_function_custom(unsigned char *output, const unsigned char *x, const unsigned char *y, void *data)
{
    (void) data;
    (void) y;

    /* Save x as uncompressed public key */
    memcpy(output, x, 32);

    return 1;
}

int Crypto_SECPK1_GenerateECKeyPair(
        CryptoECPrivateKey_t * const            outPrivateKey,
        CryptoECPublicKey_t * const             outPublicKey)
{
    int ret = --EINVAL;

    unsigned char randomize[32];
    int return_val;
    secp256k1_pubkey pubkey;
    size_t len;

    /* Before we can call actual API functions, we need to create a "context". */
    secp256k1_context * ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    ret = Crypto_GenerateRandomData(sizeof(randomize), randomize);
    require_noerr(ret, exit);

    /* Randomizing the context is recommended to protect against side-channel
     * leakage See `secp256k1_context_randomize` in secp256k1.h for more
     * information about it. This should never fail. */
    return_val = secp256k1_context_randomize(ctx, randomize);
    require_action(return_val == 1, exit, ret = --EINVAL);

    /*** Key Generation ***/

    /* If the secret key is zero or out of range (bigger than secp256k1's
     * order), we try to sample a new key. Note that the probability of this
     * happening is negligible. */
    while (1) {
        ret = Crypto_GenerateRandomData(sizeof(outPrivateKey->bytes), outPrivateKey->bytes);
        require_noerr(ret, exit);

        if (secp256k1_ec_seckey_verify(ctx, outPrivateKey->bytes) )
        {
            break;
        }
    }

    /* Public key creation using a valid context with a verified secret key should never fail */
    return_val = secp256k1_ec_pubkey_create(ctx, &pubkey, outPrivateKey->bytes);
    require_action(return_val == 1, exit, ret = --EINVAL);

    /* Serialize the pubkey in a compressed form(33 bytes). Should always return 1. */
    len = sizeof(outPublicKey->bytes);
    return_val = secp256k1_ec_pubkey_serialize(ctx, outPublicKey->bytes, &len, &pubkey, SECP256K1_EC_COMPRESSED);
    require_action(return_val == 1, exit, ret = --EINVAL);

    /* Should be the same size as the size of the output, because we passed a 33 byte array. */
    require_action(len == sizeof(outPublicKey->bytes), exit, ret = --EINVAL);

exit:
    /* This will clear everything from the context and free the memory */
    secp256k1_context_destroy(ctx);

    return ret;
}

int Crypto_SECPK1_SignDataWithECPrivateKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPrivateKey_t * const      inPrivateKey,
        CryptoECSignature_t * const             outSign)
{
    int ret = --EINVAL;
    int return_val;
    secp256k1_ecdsa_signature sig;
    uint8_t hash[ CRYPTO_HASH_SHA256_SIZE ];

    /* Before we can call actual API functions, we need to create a "context". */
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    // Compute the SHA256 hash
    return_val = mbedtls_sha256(inData, inDataLen, hash, 0);
    require_noerr(return_val, exit);

    /*** Signing ***/

    /* Generate an ECDSA signature `noncefp` and `ndata` allows you to pass a
     * custom nonce function, passing `NULL` will use the RFC-6979 safe default.
     * Signing with a valid context, verified secret key
     * and the default nonce function should never fail. */
    return_val = secp256k1_ecdsa_sign(ctx, &sig, hash, inPrivateKey->bytes, NULL, NULL);
    require_action(return_val == 1, exit, ret = --EINVAL);

    /* Serialize the signature in a compact form. Should always return 1
     * according to the documentation in secp256k1.h. */
    return_val = secp256k1_ecdsa_signature_serialize_compact(ctx, outSign->bytes, &sig);
    require_action(return_val == 1, exit, ret = --EINVAL);

    ret = 0;
exit:
    /* This will clear everything from the context and free the memory */
    secp256k1_context_destroy(ctx);

    return ret;
}

int Crypto_SECPK1_VerifySignatureWithECPublicKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPublicKey_t * const       inPublicKey,
        const CryptoECSignature_t * const       inSignature)
{
    int ret = --EINVAL;
    int ret;
    uint8_t hash[ CRYPTO_HASH_SHA256_SIZE ];

    /* Before we can call actual API functions, we need to create a "context". */
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    /*** Verification ***/
    secp256k1_ecdsa_signature sig;
    secp256k1_pubkey pubkey;

    /* Deserialize the signature. This will return 0 if the signature can't be parsed correctly. */
    ret = secp256k1_ecdsa_signature_parse_compact(ctx, &sig, inSignature->bytes);
    require_action(ret == 1, exit, ret = --EINVAL);

    /* Deserialize the public key. This will return 0 if the public key can't be parsed correctly. */
    ret = secp256k1_ec_pubkey_parse(ctx, &pubkey, inPublicKey->bytes, sizeof(inPublicKey->bytes));
    require_action(ret == 1, exit, ret = --EINVAL);

    ret = mbedtls_sha256(inData, inDataLen, hash, 0);
    require_noerr(ret, exit);

    /* Verify a signature. This will return 1 if it's valid and 0 if it's not. */
    ret = secp256k1_ecdsa_verify(ctx, &sig, hash, &pubkey);
    require_action(ret == 1, exit, ret = --EINVAL);

    ret = 0;

exit:
    /* This will clear everything from the context and free the memory */
    secp256k1_context_destroy(ctx);

    return ret;
}

int Crypto_SECPK1_GenerateECDHSharedSecret(
        const CryptoECPrivateKey_t * const      inPrivateKey,
        const CryptoECPublicKey_t * const       inPublicKey,
        CryptoECSharedSecret_t * const          outSharedSecret)
{
    int ret = --EINVAL;
    int iret;
    unsigned char randomize[32];

    /* Before we can call actual API functions, we need to create a "context". */
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    ret = Crypto_GenerateRandomData(sizeof(randomize), randomize);
    require_noerr(ret, exit);

    /* Randomizing the context is recommended to protect against side-channel
     * leakage See `secp256k1_context_randomize` in secp256k1.h for more
     * information about it. This should never fail. */
    iret = secp256k1_context_randomize(ctx, randomize);
    require_action(iret == 1, exit, ret = --EINVAL);

    /*** Creating the shared secret ***/

    /* Serialize pubkey in a compressed form (33 bytes), should always return 1 */
    secp256k1_pubkey pubkey;

    iret = secp256k1_ec_pubkey_parse(ctx, &pubkey, inPublicKey->bytes, sizeof(inPublicKey->bytes) );
    require_action(iret == 1, exit, ret = --EINVAL);

    /* Perform ECDH with seckey and pubkey. Should never fail with a verified
     * seckey and valid pubkey */
    iret = secp256k1_ecdh(ctx, outSharedSecret->bytes, &pubkey, inPrivateKey->bytes, ecdh_hash_function_custom, NULL);
    require_action(iret == 1, exit, ret = --EINVAL);

    ret = 0;

exit:
    /* This will clear everything from the context and free the memory */
    secp256k1_context_destroy(ctx);

    return ret;
}

#endif

