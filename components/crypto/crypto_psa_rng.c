/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */
#include "crypto_psa_rng.h"

#if CRYPTO_PSA_RNG

#define COMPONENT_NAME psa_rng
#include "Logging.h"
#include "AssertMacros.h"

#include <psa/crypto.h>

int Crypto_PSA_GenerateRandomData(const size_t inRandomDataLen, uint8_t * const outRandomData)
{
    int ret = -EINVAL;

    // validate input
    require(outRandomData, exit);

    psa_status_t cryptoStatus = psa_generate_random(outRandomData, inRandomDataLen);
    ret = (cryptoStatus == PSA_SUCCESS) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

int Crypto_PSA_RngInit(void)
{
    int ret = 0;

    return ret;
}

#endif

