/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_mbed_chacha20.h"

#if CRYPTO_MBED_CHACHA20

#define COMPONENT_NAME mbed_chacha20
#include "Logging.h"
#include "AssertMacros.h"
#include "mbedtls/chacha20.h"
#include "mbedtls/hkdf.h"
#include <stdbool.h>
#include <string.h>

static int _ChaCha20SetIV(
        mbedtls_chacha20_context *ctx,
        const unsigned char *nonce,
        size_t nlen,
        uint32_t counter)
{
    unsigned char iv[12] = { 0 };
    int ret = -0x006E;

    if (nlen == 8)
    {
        memcpy(iv + 4 * sizeof(unsigned char), (void *) nonce, nlen * sizeof(unsigned char) );
    }
    else if (nlen == 12)
    {
        memcpy(iv, (void *) nonce, nlen * sizeof(unsigned char) );
    }
    else
    {
        return MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA;
    }

    ret = mbedtls_chacha20_starts(ctx, iv, counter);
    return ret;
}

int Crypto_MBED_ChaChaEncrypt(
        const CryptoChaChaKey_t * const     inKey,
        const CryptoChaChaNonce_t * const   inNonce,
        const uint8_t * const               inPlainText,
        const size_t                        inPlainTextLen,
        const size_t                        inCipherTextBufferLen,
        uint8_t * const                     outCipherText)
{
    int ret = -EINVAL;
    mbedtls_chacha20_context ctx;
    int status;

    // Validate inputs
    require(inKey != NULL, exit);
    require(inNonce != NULL, exit);
    require(inPlainText != NULL, exit);
    require(inPlainTextLen > 0, exit);
    require(inCipherTextBufferLen >= inPlainTextLen, exit);
    require(outCipherText != NULL, exit);

    mbedtls_chacha20_init(&ctx);

    status = mbedtls_chacha20_setkey(&ctx, inKey->bytes);
    require_action(status == 0, exit, ret = -EINVAL);

    status = _ChaCha20SetIV(&ctx, inNonce->bytes, CRYPTO_CHACHA_NONCE_SIZE, 0U); /* Initial counter value */
    require_action(status == 0, exit, ret = -EINVAL);

    status = mbedtls_chacha20_update(&ctx, inPlainTextLen, inPlainText, outCipherText);
    require_action(status == 0, exit, ret = -EINVAL);

    ret = 0;

exit:
    mbedtls_chacha20_free(&ctx);
    return ret;
}

int Crypto_MBED_ChaChaDecrypt(
        const CryptoChaChaKey_t * const     inKey,
        const CryptoChaChaNonce_t * const   inNonce,
        const uint8_t * const               inCipherText,
        const size_t                        inCipherTextLen,
        const size_t                        inPlainTextBufferLen,
        uint8_t * const                     outPlainText)
{
    int ret = -EINVAL;
    mbedtls_chacha20_context ctx;
    int status;

    // Validate inputs
    require(inKey != NULL, exit);
    require(inNonce != NULL, exit);
    require(inCipherText != NULL, exit);
    require(inCipherTextLen > 0, exit);
    require(inPlainTextBufferLen >= inCipherTextLen, exit);
    require(outPlainText != NULL, exit);

    mbedtls_chacha20_init(&ctx);

    status = mbedtls_chacha20_setkey(&ctx, inKey->bytes);
    require_action(status == 0, exit, ret = -EINVAL);

    status = _ChaCha20SetIV(&ctx, inNonce->bytes, CRYPTO_CHACHA_NONCE_SIZE, 0U); /* Initial counter value */
    require_action(status == 0, exit, ret = -EINVAL);

    status = mbedtls_chacha20_update(&ctx, inCipherTextLen, inCipherText, outPlainText);
    require_action(status == 0, exit, ret = -EINVAL);

    ret = 0;

exit:
    mbedtls_chacha20_free(&ctx);

    return ret;
}

int Crypto_MBED_DeriveChaChaKeyUsingHKDF(
        const uint8_t * const                   inSecret,
        const size_t                            inSecretLen,
        const uint8_t * const                   inSalt,
        const size_t                            inSaltLen,
        const uint8_t * const                   inInfo,
        const size_t                            inInfoLen,
        CryptoChaChaKey_t * const               outKey)
{
    int ret = -EINVAL;

    // Validate inputs
    require(inSecret != NULL, exit);
    require(inSecretLen > 0, exit);
    require(inSalt != NULL, exit);
    require(inSaltLen > 0, exit);
    require(inInfo != NULL, exit);
    require(inInfoLen > 0, exit);
    require(outKey != NULL, exit);

    int err = mbedtls_hkdf(
            mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
            (uint8_t *) inSalt,
            inSaltLen,
            (uint8_t *) inSecret,
            inSecretLen,
            (uint8_t *) inInfo,
            inInfoLen,
            (uint8_t *) outKey,
            CRYPTO_CHACHA_KEY_SIZE);

    ret = (err == 0) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

#endif

