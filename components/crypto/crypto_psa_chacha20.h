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

#include "Crypto_Interface.h"
#include "crypto_platform.h"
#include <stdint.h>
#include <stddef.h>

int Crypto_PSA_ChaChaEncrypt(
        const CryptoChaChaKey_t * const     inKey,
        const CryptoChaChaNonce_t * const   inNonce,
        const uint8_t * const               inPlainText,
        const size_t                        inPlainTextLen,
        const size_t                        inCipherTextBufferLen,
        uint8_t * const                     outCipherText
        );

int Crypto_PSA_ChaChaDecrypt(
        const CryptoChaChaKey_t * const     inKey,
        const CryptoChaChaNonce_t * const   inNonce,
        const uint8_t * const               inCipherText,
        const size_t                        inCipherTextLen,
        const size_t                        inPlainTextBufferLen,
        uint8_t * const                     outPlainText
        );

int Crypto_PSA_DeriveChaChaKeyUsingHKDF(
        const uint8_t * const                   inSecret,
        const size_t                            inSecretLen,
        const uint8_t * const                   inSalt,
        const size_t                            inSaltLen,
        const uint8_t * const                   inInfo,
        const size_t                            inInfoLen,
        CryptoChaChaKey_t * const               outKey
        );

int Crypto_PSA_ChaChaPoly1305Encrypt(
        const CryptoChaChaKey_t * const         inKey,
        const CryptoChaChaLongNonce_t * const   inNonce,
        const uint8_t * const                   inPlainText,
        const size_t                            inPlainTextLen,
        const size_t                            inCipherTextBufferLen,
        uint8_t * const                         outCipherText,
        size_t *                                outCipherTextLen
        );

int Crypto_PSA_ChaChaPoly1305Decrypt(
        const CryptoChaChaKey_t * const         inKey,
        const CryptoChaChaLongNonce_t * const   inNonce,
        const uint8_t * const                   inCipherText,
        const size_t                            inCipherTextLen,
        const size_t                            inPlainTextBufferLen,
        uint8_t * const                         outPlainText,
        size_t *                                outPlainTextLen
        );

int Crypto_PSA_SetSecretAsChaChaKey(
        const uint8_t * const                   inSecret,
        const size_t                            inSecretLen,
        CryptoChaChaKey_t * const               outKey
        );

int Crypto_PSA_DestroySessionKey( const CryptoChaChaKey_t * const inKey );

