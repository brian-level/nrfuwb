/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#pragma once

#include "autoconf.h"

// figure out which crypto functions to use
//
// the preferred functions are the "psa" api functions provided by
// mbedtls on both host/pc builds and embedded, where the embedded
// system is usually h/w accelerated under psa
//
#if CONFIG_PSA_CRYPTO_DRIVER_OBERON
  // Oberon use deselects h/w rng use
  #define CRYPTO_MBED_RNG       (1)
  // Oberon crypto (vs. CC3XX) doesn't support secp256k1
  #define CRYPTO_MBED_ECC       (1)
  // use s/w chacha20 which is needed for the shorter nonce Level uses
  #define CRYPTO_MBED_CHACHA20  (1)
#else
  #define CRYPTO_PSA_RNG        (1)
  #define CRYPTO_PSA_ECC        (1)
  #define CRYPTO_PSA_CHACHA20   (1)
#endif

#if CRYPTO_PSA_RNG
  #define Crypto_PSA_RngInit Crypto_RngInit
  #define Crypto_PSA_GenerateRandomData           Crypto_GenerateRandomData
#elif CRYPTO_MBED_RNG
  #define Crypto_MBED_RngInit Crypto_RngInit
  #define Crypto_MBED_GenerateRandomData          Crypto_GenerateRandomData
#else
  #pragma warning("No RNG impl");
#endif

#if CRYPTO_PSA_ECC
  #define Crypto_PSA_GenerateECKeyPair               Crypto_GenerateECKeyPair
  #define Crypto_PSA_SignDataWithECPrivateKey        Crypto_SignDataWithECPrivateKey
  #define Crypto_PSA_VerifySignatureWithECPublicKey  Crypto_VerifySignatureWithECPublicKey
  #define Crypto_PSA_GenerateECDHSharedSecret        Crypto_GenerateECDHSharedSecret
#elif CRYPTO_MBED_ECC
  #define Crypto_MBED_GenerateECKeyPair               Crypto_GenerateECKeyPair
  #define Crypto_MBED_SignDataWithECPrivateKey        Crypto_SignDataWithECPrivateKey
  #define Crypto_MBED_VerifySignatureWithECPublicKey  Crypto_VerifySignatureWithECPublicKey
  #define Crypto_MBED_GenerateECDHSharedSecret        Crypto_GenerateECDHSharedSecret
#elif CRYPTO_SECPK1_eCC
  #define Crypto_SECPK1_GenerateECKeyPair               Crypto_GenerateECKeyPair
  #define Crypto_SECPK1_SignDataWithECPrivateKey        Crypto_SignDataWithECPrivateKey
  #define Crypto_SECPK1_VerifySignatureWithECPublicKey  Crypto_VerifySignatureWithECPublicKey
  #define Crypto_SECPK1_GenerateECDHSharedSecret        Crypto_GenerateECDHSharedSecret
#else
  #pragma warning("No ECC impl");
#endif

#if CRYPTO_PSA_CHACHA20
  #define Crypto_PSA_ChaChaEncrypt             Crypto_ChaChaEncrypt
  #define Crypto_PSA_ChaChaDecrypt             Crypto_ChaChaDecrypt
  #define Crypto_PSA_DeriveChaChaKeyUsingHKDF  Crypto_DeriveChaChaKeyUsingHKDF
#elif CRYPTO_MBED_CHACHA20
  #define Crypto_MBED_ChaChaEncrypt             Crypto_ChaChaEncrypt
  #define Crypto_MBED_ChaChaDecrypt             Crypto_ChaChaDecrypt
  #define Crypto_MBED_DeriveChaChaKeyUsingHKDF  Crypto_DeriveChaChaKeyUsingHKDF
#else
  #pragma warning("No CHACHA20 impl");
#endif

#define Crypto_ChaChaPoly1305Encrypt    Crypto_PSA_ChaChaPoly1305Encrypt
#define Crypto_ChaChaPoly1305Decrypt    Crypto_PSA_ChaChaPoly1305Decrypt
#define Crypto_DestroySessionKey        Crypto_PSA_DestroySessionKey
#define Crypto_SetSecretAsChaChaKey     Crypto_PSA_SetSecretAsChaChaKey

int  CryptoPlatformInit(void);
void CryptoPlatformDeinit(void);

