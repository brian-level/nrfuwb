/*
 * Copyright (c) 2022, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_psa_ecc.h"

#if CRYPTO_PSA_ECC

#include "micro_ecc.h"

#define COMPONENT_NAME psa_ecc
#include "Logging.h"
#include "AssertMacros.h"

#include <psa/crypto.h>

#include <string.h>
#include <stdbool.h>

// Types for the EC private and public keys in internal structure format
typedef psa_key_handle_t    CryptoECPublicKeyInternalStructure_t;
typedef psa_key_handle_t    CryptoECPrivateKeyInternalStructure_t;

typedef enum
{
    KeyUsage_Signing,
    KeyUsage_Derivation,
    KeyUsage_SharedSecret,
}
KeyUsage_t;

static bool IsPublicKeyCompressed(const CryptoECPublicKey_t * const inPubKey)
{
    bool ret = false;

    require(inPubKey, exit);
    if (inPubKey->bytes[0] == 0x03 || inPubKey->bytes[0] == 0x02)
    {
        ret = true;
    }
exit:
    return ret;
}

static int _ConvertPublicKeyFromInternalStructureToCompressedRawBytes(
        const CryptoECPublicKeyInternalStructure_t * const  inPublicKey,
        CryptoECPublicKey_t * const                         outPublicBytes)
{
    int ret = -EINVAL;
    size_t actualPublicKeySize = 0;

    CryptoECPublicKeyUncompressed_t uncompressedPubKey;

    // Validate inputs
    require(outPublicBytes, exit);
    require(inPublicKey, exit);

    psa_status_t cryptoStatus = psa_export_public_key(
            *inPublicKey,
            (uint8_t *) uncompressedPubKey.bytes,
            sizeof(CryptoECPublicKeyUncompressed_t),
            &actualPublicKeySize);
    ret = (cryptoStatus == PSA_SUCCESS) ? 0 : -EINVAL;
    verify_noerr(ret);

    // Convert uncompressed key to compressed
    ecc_point_compress_bytes(outPublicBytes->bytes, uncompressedPubKey.bytes);

exit:
    return ret;
}

static int _ConvertPublicKeyFromCompressedToUncompressedRawBytes(
        const CryptoECPublicKey_t * const               inPublicBytes,
              CryptoECPublicKeyUncompressed_t * const   outUncompressedPublicBytes)
{
    if (IsPublicKeyCompressed(inPublicBytes))
    {
        ecc_point_decompress_bytes((uint8_t *) outUncompressedPublicBytes->bytes, (uint8_t *) inPublicBytes->bytes);
    }
    else
    {
        memcpy(outUncompressedPublicBytes->bytes, inPublicBytes->bytes, sizeof(outUncompressedPublicBytes->bytes));
    }

    return 0;
}

static int _ConvertPublicKeyFromCompressedRawBytesToInternalStructure(
        const CryptoECPublicKey_t * const               inPublicBytes,
        CryptoECPublicKeyInternalStructure_t * const    outPublicKey)
{
    int ret = -EINVAL;
    psa_key_attributes_t keyAttributes = PSA_KEY_ATTRIBUTES_INIT;

    // Validate inputs
    require(inPublicBytes, exit);
    require(outPublicKey, exit);

    CryptoECPublicKeyUncompressed_t uncompressedPublicKey;
    _ConvertPublicKeyFromCompressedToUncompressedRawBytes(inPublicBytes, &uncompressedPublicKey);

    psa_set_key_usage_flags(&keyAttributes, (PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_DERIVE) );
    psa_set_key_lifetime(&keyAttributes, PSA_KEY_LIFETIME_VOLATILE);
    psa_set_key_algorithm(&keyAttributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256) );
    psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_K1) );
    psa_set_key_bits(&keyAttributes, 256);

    psa_status_t cryptoStatus = psa_import_key(
            &keyAttributes,
            uncompressedPublicKey.bytes,
            sizeof(CryptoECPublicKeyUncompressed_t),
            outPublicKey);
    ret = (cryptoStatus == PSA_SUCCESS) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

static int _ConvertPrivateKeyFromInternalStructureToRawBytes(
        const CryptoECPrivateKeyInternalStructure_t * const inPrivateKey,
        CryptoECPrivateKey_t * const                        outRawBytes)
{
    int ret = -EINVAL;
    size_t targetSize = CRYPTO_EC_PRIVATE_KEY_SIZE;
    size_t actualSize = 0;

    // Validate inputs
    require(inPrivateKey, exit);
    require(outRawBytes, exit);

    psa_status_t cryptoStatus = psa_export_key(
            *inPrivateKey,
            (uint8_t *) outRawBytes,
            targetSize,
            &actualSize);

    ret = (cryptoStatus == PSA_SUCCESS) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

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

int Crypto_PSA_GenerateECKeyPair(
        CryptoECPrivateKey_t * const    outPrivateKey,
        CryptoECPublicKey_t * const     outPublicKey)
{
    int ret = -EINVAL;
    CryptoECPrivateKeyInternalStructure_t keyPairHandle;
    psa_status_t cryptoStatus;
    bool keyCreated = false;

    // Validate input
    require(outPrivateKey != NULL, exit);
    require(outPublicKey != NULL, exit);

    /* Configure the key attributes */
    psa_key_attributes_t keyAttributes = PSA_KEY_ATTRIBUTES_INIT;

    /* Configure the key attributes */
    psa_set_key_usage_flags(&keyAttributes, (PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT) );
    psa_set_key_lifetime(&keyAttributes, PSA_KEY_LIFETIME_VOLATILE);
    psa_set_key_algorithm(&keyAttributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256) );
    psa_set_key_type(&keyAttributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_K1) );
    psa_set_key_bits(&keyAttributes, 256);

    /* Generate a random keypair. The keypair is not exposed to the application,
     * we can use it to signing/verification the key handle.
     */
    cryptoStatus = psa_generate_key(&keyAttributes, &keyPairHandle);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);
    keyCreated = true;

    // Convert private key from internal structure to raw bytes
    ret = _ConvertPrivateKeyFromInternalStructureToRawBytes(&keyPairHandle, outPrivateKey);
    verify_noerr(ret);

    // Convert public key from internal structure to raw bytes
    ret = _ConvertPublicKeyFromInternalStructureToCompressedRawBytes(&keyPairHandle, outPublicKey);
    verify_noerr(ret);

    /* After the key handle is acquired the attributes are not needed */
    psa_reset_key_attributes(&keyAttributes);

exit:
    if (keyCreated)
    {
        cryptoStatus = psa_destroy_key(keyPairHandle);
        verify_noerr(cryptoStatus);
    }

    return ret;
}

int Crypto_PSA_SignDataWithECPrivateKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPrivateKey_t * const      inPrivateKey,
        CryptoECSignature_t * const             outSign)
{
    int ret = -EINVAL;
    CryptoECPrivateKeyInternalStructure_t privateKeyInternalStructure;
    uint8_t hash[ CRYPTO_HASH_SHA256_SIZE ];
    size_t outputLen;
    psa_status_t cryptoStatus;
    bool keyCreated = false;

    // Validate inputs
    require(inData, exit);
    require(inDataLen != 0, exit);
    require(inPrivateKey, exit);
    require(outSign, exit);

    ret = _ConvertPrivateKeyFromRawBytesToInternalStructure(
            inPrivateKey,
            sizeof(CryptoECPrivateKey_t),
            KeyUsage_Signing,
            &privateKeyInternalStructure);
    require_noerr(ret, exit);
    keyCreated = true;

    /* Compute the SHA256 hash*/
    cryptoStatus = psa_hash_compute(
            PSA_ALG_SHA_256,
            inData,
            inDataLen,
            hash,
            sizeof(hash),
            &outputLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    /* Sign the hash */
    cryptoStatus = psa_sign_hash(
            privateKeyInternalStructure,
            PSA_ALG_ECDSA(PSA_ALG_SHA_256),
            hash,
            sizeof(hash),
            outSign->bytes,
            sizeof(CryptoECSignature_t),
            &outputLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    ret = 0;

exit:
    if (keyCreated)
    {
        cryptoStatus = psa_destroy_key(privateKeyInternalStructure);
        verify_noerr(cryptoStatus);
    }

    return ret;
}

int Crypto_PSA_VerifySignatureWithECPublicKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPublicKey_t * const       inPublicKey,
        const CryptoECSignature_t * const       inSignature)
{
    int ret = -EINVAL;
    CryptoECPublicKeyInternalStructure_t publicKeyInternalStructure;
    psa_status_t cryptoStatus;
    uint8_t hash[ CRYPTO_HASH_SHA256_SIZE ];
    size_t outputLen;
    bool keyCreated = false;

    require(inData != NULL, exit);
    require(inDataLen > 0, exit);
    require(inPublicKey != NULL, exit);
    require(inSignature != NULL, exit);

    ret = _ConvertPublicKeyFromCompressedRawBytesToInternalStructure(inPublicKey, &publicKeyInternalStructure);
    require_noerr(ret, exit);
    keyCreated = true;

    /* Compute the SHA256 hash*/
    cryptoStatus = psa_hash_compute(
            PSA_ALG_SHA_256,
            inData,
            inDataLen,
            hash,
            sizeof(hash),
            &outputLen);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    cryptoStatus = psa_verify_hash(
            publicKeyInternalStructure,
            PSA_ALG_ECDSA(PSA_ALG_SHA_256),
            hash,
            sizeof(hash),
            inSignature->bytes,
            sizeof(CryptoECSignature_t) );
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    ret = 0;

exit:
    if (keyCreated)
    {
        cryptoStatus = psa_destroy_key(publicKeyInternalStructure);
        verify_noerr(cryptoStatus);
    }

    return ret;
}

int Crypto_PSA_GenerateECDHSharedSecret(
        const CryptoECPrivateKey_t * const      inPrivateKey,
        const CryptoECPublicKey_t * const       inPublicKey,
        CryptoECSharedSecret_t * const          outSharedSecret)
{
    int ret = -EINVAL;
    size_t outLength;
    psa_status_t cryptoStatus;
    bool keyCreated = false;

    // Validate inputs
    require(inPrivateKey, exit);
    require(inPublicKey, exit);
    require(outSharedSecret, exit);

    CryptoECPublicKeyUncompressed_t publicKeyUncompressed;
    CryptoECPrivateKeyInternalStructure_t privateKeyInternal;

    // Convert private key from raw bytes to internal structure
    ret = _ConvertPrivateKeyFromRawBytesToInternalStructure(
            inPrivateKey,
            sizeof(CryptoECPrivateKey_t),
            KeyUsage_SharedSecret,
            &privateKeyInternal);
    require_noerr(ret, exit);
    keyCreated = true;

    // Convert public key from compressed to uncompressed raw bytes
    ret = _ConvertPublicKeyFromCompressedToUncompressedRawBytes(inPublicKey, &publicKeyUncompressed);
    verify_noerr(ret);

    /* Perform the ECDH key exchange to calculate the secret */
    cryptoStatus = psa_raw_key_agreement(
            PSA_ALG_ECDH,
            privateKeyInternal,
            publicKeyUncompressed.bytes,
            sizeof(CryptoECPublicKeyUncompressed_t),
            outSharedSecret->bytes,
            sizeof(CryptoECSharedSecret_t),
            &outLength);
    require_action(cryptoStatus == PSA_SUCCESS, exit, ret = -EINVAL);

    ret = 0;

exit:
    if (keyCreated)
    {
        cryptoStatus = psa_destroy_key(privateKeyInternal);
        verify_noerr(cryptoStatus);
    }

    return ret;
}

#endif

