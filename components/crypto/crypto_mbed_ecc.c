/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_mbed_ecc.h"

#if CRYPTO_MBED_ECC

#define COMPONENT_NAME mbed_ecc
#include "Logging.h"
#include "AssertMacros.h"

// mbedtls based ecc depends on mbedtls based rng as well
#include "crypto_mbed_rng.h"

#include <mbedtls/sha256.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/pk.h>
#include <mbedtls/asn1.h>
#include <mbedtls/md.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/ecp.h>
#include "mbedtls/bignum.h"
#include <mbedtls/cipher.h>
#include <mbedtls/cmac.h>
#include <mbedtls/aes.h>

#include <string.h>
#include <stdbool.h>

static bool _IsPublicKeyCompressed(const CryptoECPublicKey_t * const inPubKey)
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

static int _EcpCompress(
    const mbedtls_ecp_group *grp,
    const unsigned char *input, size_t ilen,
    unsigned char *output, size_t *olen, size_t osize)
{
    size_t plen;

    plen = mbedtls_mpi_size(&grp->P);

    *olen = plen + 1;

    if (osize < *olen)
        return(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);

    if (ilen != 2 * plen + 1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (input[0] != 0x04)
        return(MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    // output will consist of 0x0?|X
    memcpy(output, input, *olen);

    // Encode even/odd of Y into first byte (either 0x02 or 0x03)
    output[0] = 0x02 + (input[2 * plen] & 1);

    return(0);
}

static int _EcpDecompress(
    const mbedtls_ecp_group *grp,
    const unsigned char *input, size_t ilen,
    unsigned char *output, size_t *olen, size_t osize)
{
    int ret;
    size_t plen;
    mbedtls_mpi r;
    mbedtls_mpi x;
    mbedtls_mpi n;

    plen = mbedtls_mpi_size(&grp->P);

    *olen = 2 * plen + 1;

    if (osize < *olen)
    {
        return(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);
    }

    if (ilen != plen + 1)
    {
        return(MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }

    if (input[0] != 0x02 && input[0] != 0x03)
    {
        return(MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }

    // output will consist of 0x04|X|Y
    memcpy(output, input, ilen);
    output[0] = 0x04;

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&x);
    mbedtls_mpi_init(&n);

    // x <= input
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&x, input + 1, plen) );

    // r = x^2
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&r, &x, &x) );

    // r = x^2 + a
    if (grp->A.MBEDTLS_PRIVATE(p) == NULL)
    {
        // Special case where a is -3
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&r, &r, 3) );
    }
    else
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&r, &r, &grp->A) );
    }

    // r = x^3 + ax
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&r, &r, &x) );

    // r = x^3 + ax + b
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&r, &r, &grp->B) );

    // Calculate square root of r over finite field P:
    //   r = sqrt(x^3 + ax + b) = (x^3 + ax + b) ^ ((P + 1) / 4) (mod P)

    // n = P + 1
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&n, &grp->P, 1) );

    // n = (P + 1) / 4
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&n, 2) );

    // r ^ ((P + 1) / 4) (mod p)
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&r, &r, &n, &grp->P, NULL) );

    // Select solution that has the correct "sign" (equals odd/even solution in finite group)
    if (( input[0] == 0x03) != mbedtls_mpi_get_bit(&r, 0) )
    {
        // r = p - r
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&r, &grp->P, &r) );
    }

    // y => output
    ret = mbedtls_mpi_write_binary(&r, output + 1 + plen, plen);

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&x);
    mbedtls_mpi_free(&n);

    return(ret);
}

static int _GetMPI(uint8_t **inASN1, uint8_t *inEnd, uint8_t *outBits, size_t *outLen)
{
    int ret;
    size_t len;

    *outLen = 0;

    if(( ret = mbedtls_asn1_get_tag(inASN1, inEnd, &len, MBEDTLS_ASN1_INTEGER) ) != 0)
    {
        return ret;
    }

    memcpy(outBits, *inASN1, len);
    *inASN1 += len;
    *outLen = len;
    return ret;
}

int Crypto_MBED_GenerateECKeyPair(
        CryptoECPrivateKey_t * const    outPrivateKey,
        CryptoECPublicKey_t * const     outPublicKey)
{
    int ret = -EINVAL;
    CryptoECPublicKeyUncompressed_t publicKey;
    size_t compressedLen;
    int cryptoStatus;

    unsigned char private_key[128];
    unsigned char public_key[128];

    // Validate input
    require(outPrivateKey != NULL, exit);
    require(outPublicKey != NULL, exit);

    mbedtls_pk_context keypair;
    mbedtls_pk_init(&keypair);

    mbedtls_pk_context priv_key;
    mbedtls_pk_init(&priv_key);

    ret = -EINVAL;

    // Setup for ECC key
    cryptoStatus = mbedtls_pk_setup(&keypair, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY) );
    require_noerr(cryptoStatus, exit);

    // Generate an ECC key
    cryptoStatus = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256K1, mbedtls_pk_ec(keypair), mbedtls_ctr_drbg_random, &host_drbg_context);
    require_noerr(cryptoStatus, exit);

    int private_key_len, public_key_len;

    // Write private key into buffer
    private_key_len = mbedtls_pk_write_key_der(&keypair, private_key, sizeof(private_key) );
    require(private_key_len > 0, exit);

    /* Parse and get binary priv key */
    cryptoStatus = mbedtls_pk_parse_key(
            &priv_key,
            private_key + sizeof(private_key) - private_key_len,
            private_key_len,
            NULL,
            0,
            mbedtls_ctr_drbg_random,
            &host_drbg_context);
    require_noerr(cryptoStatus, exit);

    const mbedtls_ecp_keypair *ecp_keypair = mbedtls_pk_ec(priv_key);

    cryptoStatus = mbedtls_mpi_write_binary(
            &(ecp_keypair->private_d),
            outPrivateKey->bytes,
            sizeof(CryptoECPrivateKey_t) );
    require_noerr(cryptoStatus, exit);

    // Write public key into buffer
    public_key_len = mbedtls_pk_write_pubkey_der(&keypair, public_key, sizeof(public_key) );
    require(public_key_len > 0, exit);

    mbedtls_ecp_keypair * ec_keypair = mbedtls_pk_ec(keypair);
    size_t buffer_lenX = (mbedtls_mpi_bitlen(&ec_keypair->private_Q.private_X) + 7) / 8;
    size_t buffer_lenY = (mbedtls_mpi_bitlen(&ec_keypair->private_Q.private_X) + 7) / 8;

    // The prefix is fixed 0x04 for uncompressed keys
    publicKey.bytes[ 0 ] = 0x04;

    // Get X coordinate
    cryptoStatus = mbedtls_mpi_write_binary(&ec_keypair->private_Q.private_X, publicKey.bytes + 1, buffer_lenX);
    require_noerr(cryptoStatus, exit);

    // Get Y coordinate
    cryptoStatus = mbedtls_mpi_write_binary(&ec_keypair->private_Q.private_Y, publicKey.bytes + 1 + buffer_lenX, buffer_lenY);
    require_noerr(cryptoStatus, exit);

    // compress key
    cryptoStatus = _EcpCompress(&ec_keypair->MBEDTLS_PRIVATE(grp), publicKey.bytes,
                        1 + buffer_lenX + buffer_lenY,
                        outPublicKey->bytes, &compressedLen, sizeof(outPublicKey->bytes));
    require_noerr(cryptoStatus, exit);

    ret = 0;

exit:
    return ret;
}

int Crypto_MBED_SignDataWithECPrivateKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPrivateKey_t * const      inPrivateKey,
        CryptoECSignature_t * const             outSign)
{
    int ret = -EINVAL;
    uint8_t hash[ CRYPTO_HASH_SHA256_SIZE ];
    int cryptoStatus;

    // Validate inputs
    require(inData, exit);
    require(inDataLen != 0, exit);
    require(inPrivateKey, exit);
    require(outSign, exit);

    ret = -EINVAL;

    mbedtls_ecdsa_context ctx_sign;
    mbedtls_ecdsa_init(&ctx_sign);

    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);

    cryptoStatus = mbedtls_ecp_group_load(&keypair.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256K1); // Choose the appropriate curve
    require_noerr(cryptoStatus, exit);

    // Load the EC private key
    cryptoStatus = mbedtls_mpi_read_binary(
            &keypair.MBEDTLS_PRIVATE(d),
            inPrivateKey->bytes,
            sizeof(CryptoECPrivateKey_t) );
    require_noerr(cryptoStatus, exit);

    // Bind the EC group and public key to the ECDSA context
    cryptoStatus = mbedtls_ecdsa_from_keypair(&ctx_sign, &keypair);
    require_noerr(cryptoStatus, exit);

    // Compute the SHA256 hash
    cryptoStatus = mbedtls_sha256(inData, inDataLen, hash, 0);
    require_noerr_action(cryptoStatus, exit, ret = -EINVAL);

    // Sign the hash
    size_t signature_length;
    uint8_t signature_buffer[ CRYPTO_EC_SIGNATURE_SIZE_IN_BYTES + 16 ];
    uint8_t signature_raw[ CRYPTO_EC_SIGNATURE_SIZE_IN_BYTES + 3 ];

    cryptoStatus = mbedtls_ecdsa_write_signature(
            &ctx_sign,
            MBEDTLS_MD_SHA256,
            hash,
            CRYPTO_HASH_SHA256_SIZE,
            signature_buffer,
            CRYPTO_EC_SIGNATURE_SIZE_IN_BYTES + 16,
            &signature_length,
            mbedtls_ctr_drbg_random,
            &host_drbg_context);
    require_noerr(cryptoStatus, exit);

    // convert asn1 signature back to raw
    uint8_t *pasn = signature_buffer;
    uint8_t *pend = signature_buffer + signature_length;
    uint8_t *pout = signature_raw;
    size_t mpilen;

    // this is kind of a hand-made asn.1 parser with mbedtls asn.1 helpers
    //
    // the output of signing is the two signature parts concatendated as
    // a sequence of ints.  The are unsigned ints, so if they happen to
    // have the MSB set they are prepended with 0x00 and are 33 bytes not 32
    //
    require(pasn[0] == (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE), exit);

    pasn++;
    signature_length = *pasn++;

    cryptoStatus = _GetMPI(&pasn, pend, pout, &mpilen);
    require_noerr(cryptoStatus, exit);
    require(mpilen <= 33, exit);
    if (mpilen == 33)
    {
        memmove(pout, pout + 1, 32);
        mpilen = 32;
    }
    pout += mpilen;

    cryptoStatus = _GetMPI(&pasn, pend, pout, &mpilen);
    require_noerr(cryptoStatus, exit);
    require(mpilen <= 33, exit);
    if (mpilen == 33)
    {
        memmove(pout, pout + 1, 32);
        mpilen = 32;
    }

    memcpy(outSign->bytes, signature_raw, CRYPTO_EC_SIGNATURE_SIZE_IN_BYTES);

    ret = 0;

exit:
    mbedtls_ecp_keypair_free(&keypair);
    mbedtls_ecdsa_free(&ctx_sign);
    return ret;
}

int Crypto_MBED_VerifySignatureWithECPublicKey(
        const uint8_t * const                           inData,
        const size_t                                    inDataLen,
        const CryptoECPublicKey_t * const               inPublicKey,
        const CryptoECSignature_t * const               inSignature)
{
    int ret = -EINVAL;
    uint8_t hash[ CRYPTO_HASH_SHA256_SIZE ];
    int err;

    require(inData != NULL, exit);
    require(inDataLen > 0, exit);
    require(inPublicKey != NULL, exit);
    require(inSignature != NULL, exit);

    ret = -EINVAL;

    err = mbedtls_sha256(inData, inDataLen, hash, 0);
    require_noerr(err, exit);

    // convert signature, which is two 32 byte numbers
    // into asn1.1 format that mbedtls expects
    //
    uint8_t asnSignature[ MBEDTLS_ECDSA_MAX_LEN ];
    int siglen = 0;
    int totlenpos;

    asnSignature[ siglen++ ] = MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE; // 0x30
    totlenpos = siglen;
    asnSignature[ siglen++ ] = 0; // total len - will be populated after

    // copy first 32 bytes with ASN1 encoding for tag + length
    // extra empty byte appended if first byte of first half of signature is > 0x80
    asnSignature[ siglen++ ] = 0x02;
    if (inSignature->bytes[0] & 0x80)
    {
        asnSignature[ siglen++ ] = 33;
        asnSignature[ siglen++ ] = 0;
    }
    else
    {
        asnSignature[ siglen++ ] = 32;
    }
    memcpy(asnSignature + siglen, inSignature->bytes, 32);
    siglen += 32;

    // copy second 32 bytes with ASN1 encoding for tag + length
    // extra empty byte appended if first byte of second half of signature is > 0x80
    asnSignature[siglen++] = 0x02;
    if (inSignature->bytes[ 32 ] & 0x80)
    {
        asnSignature[ siglen++ ] = 33;
        asnSignature[ siglen++ ] = 0;
    }
    else
    {
        asnSignature[siglen++] = 32;
    }
    memcpy(asnSignature + siglen, inSignature->bytes + 32, 32);
    siglen += 32;

    // populate the total len
    asnSignature[totlenpos] = siglen - totlenpos - 1;

    mbedtls_ecdsa_context ctx_verify;
    mbedtls_ecdsa_init(&ctx_verify);

    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);

    mbedtls_ecp_group_init(&keypair.MBEDTLS_PRIVATE(grp) );
    err = mbedtls_ecp_group_load(&keypair.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256K1);
    require_noerr(err, exit);

    size_t actualPublicKeySize = 0;
    CryptoECPublicKeyUncompressed_t uncompressedPublicKey = { 0 };

    if (_IsPublicKeyCompressed(inPublicKey))
    {
        err = _EcpDecompress(
                &keypair.MBEDTLS_PRIVATE(grp),
                inPublicKey->bytes,
                sizeof(inPublicKey->bytes),
                uncompressedPublicKey.bytes,
                &actualPublicKeySize,
                sizeof(uncompressedPublicKey.bytes));
        require_noerr(err, exit);
    }
    else
    {
        memcpy(uncompressedPublicKey.bytes, inPublicKey->bytes, sizeof(uncompressedPublicKey.bytes));
        actualPublicKeySize = sizeof(uncompressedPublicKey.bytes);
    }

    // Load the EC public key
    err = mbedtls_ecp_point_read_binary(
            &keypair.MBEDTLS_PRIVATE(grp),
            &keypair.MBEDTLS_PRIVATE(Q),
            uncompressedPublicKey.bytes,
            actualPublicKeySize);
    require_noerr(err, exit);

    // Bind the EC group and public key to the ECDSA context
    err = mbedtls_ecdsa_from_keypair(&ctx_verify, &keypair);
    require_noerr(err, exit);

    err = mbedtls_ecdsa_read_signature(
            &ctx_verify,
            hash,
            sizeof(hash),
            asnSignature,
            siglen);
    require_noerr(err, exit);

    ret = 0;

exit:
    mbedtls_ecp_keypair_free(&keypair);
    mbedtls_ecdsa_free(&ctx_verify);
    return ret;
}

#ifdef LEVEL_ECP_ALT_PER_CURVE
// this is a patch made to mbedtls which allows both h/w and s/w versions of ecc to coexist
// using one for secp256k1 and another for other curves
// see patch file for reference
int mbedtls_ecdh_compute_shared_sw(mbedtls_ecp_group *grp, mbedtls_mpi *z,
                         const mbedtls_ecp_point *Q, const mbedtls_mpi *d,
                         int (*f_rng)(void *, unsigned char *, size_t),
                         void *p_rng);
#else
#define mbedtls_ecdh_compute_shared_sw mbedtls_ecdh_compute_shared
#endif

int Crypto_MBED_GenerateECDHSharedSecret(
        const CryptoECPrivateKey_t * const      inPrivateKey,
        const CryptoECPublicKey_t * const       inPublicKey,
        CryptoECSharedSecret_t * const          outSharedSecret)
{
    int ret = -EINVAL;

    // Validate inputs
    require(inPrivateKey, exit);
    require(inPublicKey, exit);
    require(outSharedSecret, exit);

    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);

    mbedtls_mpi mpi_secret;
    mbedtls_mpi_init(&mpi_secret);

    // Choose the appropriate curve
    int err = mbedtls_ecp_group_load(&keypair.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256K1);
    require_noerr(err, exit);

    size_t actualPublicKeySize = 0;
    CryptoECPublicKeyUncompressed_t uncompressedPublicKey;

    if (_IsPublicKeyCompressed(inPublicKey))
    {
        err = _EcpDecompress(
                &keypair.MBEDTLS_PRIVATE(grp),
                inPublicKey->bytes,
                sizeof(inPublicKey->bytes),
                uncompressedPublicKey.bytes,
                &actualPublicKeySize,
                sizeof(uncompressedPublicKey.bytes));
        require_noerr(err, exit);
    }
    else
    {
        memcpy(uncompressedPublicKey.bytes, inPublicKey->bytes, sizeof(uncompressedPublicKey.bytes));
        actualPublicKeySize = sizeof(uncompressedPublicKey.bytes);
    }

    // Load the EC public key
    err = mbedtls_ecp_point_read_binary(
            &keypair.MBEDTLS_PRIVATE(grp),
            &keypair.MBEDTLS_PRIVATE(Q),
            uncompressedPublicKey.bytes,
            actualPublicKeySize);
    require_noerr(err, exit);

    // Load the EC private key
    err = mbedtls_mpi_read_binary(
            &keypair.MBEDTLS_PRIVATE(d),
            inPrivateKey->bytes,
            sizeof(CryptoECPrivateKey_t) );
    require_noerr(err, exit);

    err = mbedtls_ecdh_compute_shared_sw(
        &keypair.MBEDTLS_PRIVATE(grp),
        &mpi_secret,
        &keypair.MBEDTLS_PRIVATE(Q),
        &keypair.MBEDTLS_PRIVATE(d),
        mbedtls_ctr_drbg_random,
        &host_drbg_context);
    require_noerr(err, exit);

    err = mbedtls_mpi_write_binary(&mpi_secret, outSharedSecret->bytes, sizeof(CryptoECSharedSecret_t) );
    require_noerr(err, exit);

    ret = 0;

exit:
    mbedtls_ecp_keypair_free(&keypair);
    mbedtls_mpi_free(&mpi_secret);
    return ret;
}

#endif

