/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_psa_chacha20.h"

#define COMPONENT_NAME psa_chacha20
#include "Logging.h"
#include "AssertMacros.h"

#include <psa/crypto.h>
#include <psa/crypto_extra.h>

#include <stdbool.h>
#include <string.h>

// Types for the EC private and public keys in internal structure format
typedef psa_key_handle_t    CryptoECPublicKeyInternalStructure_t;
typedef psa_key_handle_t    CryptoECPrivateKeyInternalStructure_t;

typedef enum
{
    KeyUsage_Signing,
    KeyUsage_Derivation,
    KeyUsage_SharedSecret,
    KeyUsage_Poly1305
}
KeyUsage_t;

static int _ConvertPrivateKeyFromRawBytesToInternalStructure(
        const CryptoECPrivateKey_t * const              inPrivateBytes,
        uint8_t                                         inPrivateBytesLen,
        const KeyUsage_t                                inUsage,
        CryptoECPrivateKeyInternalStructure_t * const   outPrivateKey)
{
    int ret = -EINVAL;
    psa_key_attributes_t keyAttributes = PSA_KEY_ATTRIBUTES_INIT;

    require(inPrivateBytes != NULL, exit);
    require(outPrivateKey != NULL, exit);

    psa_set_key_lifetime(&keyAttributes, PSA_KEY_LIFETIME_VOLATILE);

    switch (inUsage)
    {
    case KeyUsage_Signing:
        psa_set_key_usage_flags(&keyAttributes, PSA_KEY_USAGE_SIGN_HASH);
        psa_set_key_algorithm(&keyAttributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256) );
        psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_K1) );
        break;

    case KeyUsage_SharedSecret:
        psa_set_key_usage_flags(&keyAttributes, PSA_KEY_USAGE_DERIVE);
        psa_set_key_algorithm(&keyAttributes, PSA_ALG_ECDH);
        psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_K1) );
        break;

    case KeyUsage_Derivation:
        psa_set_key_usage_flags(&keyAttributes, PSA_KEY_USAGE_DERIVE);
        psa_set_key_algorithm(&keyAttributes, PSA_ALG_HKDF(PSA_ALG_SHA_256) );
        psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_DERIVE);
        break;

    case KeyUsage_Poly1305:
        psa_set_key_usage_flags(&keyAttributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
        psa_set_key_lifetime(&keyAttributes, PSA_KEY_LIFETIME_VOLATILE);
        psa_set_key_algorithm(&keyAttributes, PSA_ALG_CHACHA20_POLY1305);
        psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_CHACHA20);
        psa_set_key_bits(&keyAttributes, CRYPTO_CHACHA_KEY_SIZE * 8);
        break;
    }

    psa_status_t cryptoStatus = psa_import_key(
            &keyAttributes,
            inPrivateBytes->bytes,
            inPrivateBytesLen,
            outPrivateKey);

    ret = (cryptoStatus == PSA_SUCCESS) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

int Crypto_PSA_ChaChaEncrypt(
        const CryptoChaChaKey_t * const     inKey,
        const CryptoChaChaNonce_t * const   inNonce,
        const uint8_t * const               inPlainText,
        const size_t                        inPlainTextLen,
        const size_t                        inCipherTextBufferLen,
        uint8_t * const                     outCipherText)
{
    int ret = -EINVAL;

    // Validate inputs
    require(inKey != NULL, exit);
    require(inNonce != NULL, exit);
    require(inPlainText != NULL, exit);
    require(inPlainTextLen > 0, exit);
    require(inCipherTextBufferLen >= inPlainTextLen, exit);
    require(outCipherText != NULL, exit);

    psa_cipher_operation_t handle = psa_cipher_operation_init();
    psa_status_t cryptoStatus;
    size_t outputLength = 0;

    /* Setup the encryption object */
    cryptoStatus = psa_cipher_encrypt_setup(&handle, *(( psa_key_handle_t *) inKey->bytes), PSA_ALG_STREAM_CIPHER);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Set the nonce */
    cryptoStatus = psa_cipher_set_iv(&handle, inNonce->bytes, CRYPTO_CHACHA_NONCE_SIZE);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    cryptoStatus = psa_cipher_update(
            &handle,
            inPlainText,
            inPlainTextLen,
            outCipherText,
            inCipherTextBufferLen,
            &outputLength);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Finalise the cipher operation */
    cryptoStatus = psa_cipher_finish(
            &handle,
            &outCipherText[outputLength],
            inCipherTextBufferLen - outputLength,
            &outputLength);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);
    require_action(outputLength == 0, exit, ret = -EINVAL);

    ret = 0;

exit:
    cryptoStatus = psa_cipher_abort(&handle);
    verify_action(cryptoStatus == PSA_SUCCESS, ret = -EINVAL);

    return ret;
}

int Crypto_PSA_ChaChaDecrypt(
        const CryptoChaChaKey_t * const     inKey,
        const CryptoChaChaNonce_t * const   inNonce,
        const uint8_t * const               inCipherText,
        const size_t                        inCipherTextLen,
        const size_t                        inPlainTextBufferLen,
        uint8_t * const                     outPlainText)
{
    int ret = -EINVAL;

    // Validate inputs
    require(inKey != NULL, exit);
    require(inNonce != NULL, exit);
    require(inCipherText != NULL, exit);
    require(inCipherTextLen > 0, exit);
    require(inPlainTextBufferLen >= inCipherTextLen, exit);
    require(outPlainText != NULL, exit);

    psa_cipher_operation_t handle = psa_cipher_operation_init();
    psa_status_t cryptoStatus;
    size_t outputLength = 0;

    /* Setup the encryption object */
    cryptoStatus = psa_cipher_decrypt_setup(&handle, *(( psa_key_handle_t *) inKey->bytes), PSA_ALG_STREAM_CIPHER);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Set the nonce */
    cryptoStatus = psa_cipher_set_iv(&handle, inNonce->bytes, CRYPTO_CHACHA_NONCE_SIZE);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    cryptoStatus = psa_cipher_update(
            &handle,
            inCipherText,
            inCipherTextLen,
            outPlainText,
            inPlainTextBufferLen,
            &outputLength);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Finalise the cipher operation */
    cryptoStatus = psa_cipher_finish(
            &handle,
            &outPlainText[outputLength],
            inPlainTextBufferLen - outputLength,
            &outputLength);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    ret = 0;

exit:
    /* Clean up cipher operation context */
    cryptoStatus = psa_cipher_abort(&handle);
    verify_action(cryptoStatus == PSA_SUCCESS, ret = -EINVAL);

    return ret;
}

int Crypto_PSA_ChaChaPoly1305Encrypt(
        const CryptoChaChaKey_t * const         inKey,
        const CryptoChaChaLongNonce_t * const   inNonce,
        const uint8_t * const                   inPlainText,
        const size_t                            inPlainTextLen,
        const size_t                            inCipherTextBufferLen,
        uint8_t * const                         outCipherText,
        size_t *                                outCipherTextLen)
{
    int ret = -EINVAL;
    psa_status_t cryptoStatus;
    psa_key_handle_t * chaChaKeyHandle = (psa_key_handle_t *) inKey->bytes;

    // Validate inputs
    require(inKey != NULL, exit);
    require(inNonce != NULL, exit);
    require(inPlainText != NULL, exit);
    require(inPlainTextLen > 0, exit);
    require(inCipherTextBufferLen >= inPlainTextLen, exit);
    require(outCipherText != NULL, exit);
    require(outCipherTextLen != NULL, exit);

    cryptoStatus = psa_aead_encrypt(
                              *chaChaKeyHandle,
                              PSA_ALG_CHACHA20_POLY1305,
                              inNonce->bytes,
                              CRYPTO_CHACHA_LONG_NONCE_SIZE,
                              NULL,
                              0,
                              inPlainText,
                              inPlainTextLen,
                              outCipherText,
                              inCipherTextBufferLen,
                              outCipherTextLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);
    ret = 0;

exit:
    return ret;
}

int Crypto_PSA_ChaChaPoly1305Decrypt(
        const CryptoChaChaKey_t * const         inKey,
        const CryptoChaChaLongNonce_t * const   inNonce,
        const uint8_t * const                   inCipherText,
        const size_t                            inCipherTextLen,
        const size_t                            inPlainTextBufferLen,
        uint8_t * const                         outPlainText,
        size_t *                                outPlainTextLen)
{
    int ret = -EINVAL;
    psa_status_t cryptoStatus;
    psa_key_handle_t * chaChaKeyHandle = (psa_key_handle_t *) inKey->bytes;

    // Validate inputs
    require(inKey != NULL, exit);
    require(inNonce != NULL, exit);
    require(inCipherText != NULL, exit);
    require(inCipherTextLen > 0, exit);
    require(inPlainTextBufferLen >= inCipherTextLen, exit);
    require(outPlainText != NULL, exit);
    require(outPlainTextLen != NULL, exit);

    cryptoStatus = psa_aead_decrypt(
                              *chaChaKeyHandle,
                              PSA_ALG_CHACHA20_POLY1305,
                              inNonce->bytes,
                              CRYPTO_CHACHA_LONG_NONCE_SIZE,
                              NULL,
                              0,
                              inCipherText,
                              inCipherTextLen,
                              outPlainText,
                              inPlainTextBufferLen,
                              outPlainTextLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);
    ret = 0;

exit:
    return ret;
}

int Crypto_PSA_DeriveChaChaKeyUsingHKDF(
        const uint8_t * const                   inSecret,
        const size_t                            inSecretLen,
        const uint8_t * const                   inSalt,
        const size_t                            inSaltLen,
        const uint8_t * const                   inInfo,
        const size_t                            inInfoLen,
        CryptoChaChaKey_t * const               outKey)
{
    int ret = -EINVAL;
    psa_key_attributes_t keyAttributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t cryptoStatus;
    psa_key_handle_t keyHandle;
    bool keyCreated = false;

    // Validate inputs
    require(inSecret != NULL, exit);
    require(inSecretLen > 0, exit);
    require(inSalt != NULL, exit);
    require(inSaltLen > 0, exit);
    require(inInfo != NULL, exit);
    require(inInfoLen > 0, exit);
    require(outKey != NULL, exit);

    ret = _ConvertPrivateKeyFromRawBytesToInternalStructure(
            (CryptoECPrivateKey_t *) inSecret,
            inSecretLen,
            KeyUsage_Derivation,
            &keyHandle);
    require_noerr(ret, exit);
    keyCreated = true;

    psa_set_key_usage_flags(&keyAttributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_lifetime(&keyAttributes, PSA_KEY_LIFETIME_VOLATILE);
    psa_set_key_algorithm(&keyAttributes, PSA_ALG_STREAM_CIPHER);
    psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_CHACHA20);
    psa_set_key_bits(&keyAttributes, CRYPTO_CHACHA_KEY_SIZE * 8);

    /* Set the derivation algorithm */
    cryptoStatus = psa_key_derivation_setup(&operation, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Set the salt for the operation */
    cryptoStatus = psa_key_derivation_input_bytes(
            &operation,
            PSA_KEY_DERIVATION_INPUT_SALT,
            inSalt,
            inSaltLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Set the master key for the operation */
    cryptoStatus = psa_key_derivation_input_key(
            &operation,
            PSA_KEY_DERIVATION_INPUT_SECRET,
            keyHandle);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Set the additional info for the operation */
    cryptoStatus = psa_key_derivation_input_bytes(
            &operation,
            PSA_KEY_DERIVATION_INPUT_INFO,
            inInfo,
            inInfoLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Store the derived key in the keystore slot pointed by out_key_handle */
    psa_key_handle_t * chaChaKeyHandle = (psa_key_handle_t *) outKey->bytes;
    cryptoStatus = psa_key_derivation_output_key(&keyAttributes, &operation, chaChaKeyHandle);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Clean up the context */
    cryptoStatus = psa_key_derivation_abort(&operation);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    // Successful if we got to this point
    ret = 0;

exit:
    if (keyCreated)
    {
        cryptoStatus = psa_destroy_key(keyHandle);
        verify_noerr(cryptoStatus);
    }

    return ret;
}

int Crypto_PSA_DestroySessionKey(const CryptoChaChaKey_t * const inKey)
{
    int ret = -EINVAL;

    require(inKey, exit);

    psa_status_t cryptoStatus = psa_destroy_key(*((psa_key_handle_t *) inKey->bytes) );
    ret = (cryptoStatus == PSA_SUCCESS) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

int Crypto_PSA_SetSecretAsChaChaKey(
        const uint8_t * const                   inSecret,
        const size_t                            inSecretLen,
        CryptoChaChaKey_t * const               outKey)
{
    int ret = -EINVAL;
    psa_key_handle_t keyHandle;
    psa_key_handle_t * chaChaKeyHandle = (psa_key_handle_t *) outKey->bytes;

    // Validate inputs
    require(inSecret != NULL, exit);
    require(inSecretLen > 0, exit);
    require(outKey, exit);

    *chaChaKeyHandle = 0;

    ret = _ConvertPrivateKeyFromRawBytesToInternalStructure(
            (CryptoECPrivateKey_t *) inSecret,
            inSecretLen,
            KeyUsage_Poly1305,
            &keyHandle);
    require_noerr(ret, exit);

    *chaChaKeyHandle = keyHandle;

    ret = 0;
exit:
    return ret;
}

