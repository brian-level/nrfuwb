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
#include <stddef.h>

int Crypto_PSA_GenerateECKeyPair(
        CryptoECPrivateKey_t * const            outPrivateKey,
        CryptoECPublicKey_t * const             outPublicKey
        );

int Crypto_PSA_SignMessageWithECPrivateKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPrivateKey_t * const      inPrivateKey,
        CryptoECSignature_t * const             outSign
        );

int Crypto_PSA_VerifySignatureWithECPublicKey(
        const uint8_t * const                   inData,
        const size_t                            inDataLen,
        const CryptoECPublicKey_t * const       inPublicKey,
        const CryptoECSignature_t * const       inSignature
        );

int Crypto_PSA_GenerateECDHSharedSecret(
        const CryptoECPrivateKey_t * const      inPrivateKey,
        const CryptoECPublicKey_t * const       inPublicKey,
        CryptoECSharedSecret_t * const          outSharedSecret
        );

