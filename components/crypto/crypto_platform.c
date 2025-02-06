/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_platform.h"

#define COMPONENT_NAME crypto_plat
#include "Logging.h"
#include "AssertMacros.h"

#if defined(CRYPTO_PSA_RNG) || defined(CRYPTO_PSA_CHACHA20) || defined(CRYPTO_PSA_ECC)
#include <psa/crypto.h>
#endif
#ifdef CRYPTO_MBED_RNG
#include "crypto_mbed_rng.h"
#endif

int CryptoPlatformInit(void)
{
    int ret = 0;

#ifdef CRYPTO_MBED_RNG
    ret = Crypto_MBED_RngInit();
    require_noerr(ret, exit);
#endif

#if defined(CRYPTO_PSA_RNG) || defined(CRYPTO_PSA_CHACHA20) || defined(CRYPTO_PSA_ECC)
    ret = psa_crypto_init();
    require_noerr(ret, exit);
#endif

exit:
    return ret;
}

void CryptoPlatformDeinit(void)
{
    // Nothing to do
}
