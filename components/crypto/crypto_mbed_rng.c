/*
 * Copyright (c) 2023, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "crypto_mbed_rng.h"

#if CRYPTO_MBED_RNG

#define COMPONENT_NAME mbed_rng
#include "Logging.h"
#include "AssertMacros.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

mbedtls_ctr_drbg_context host_drbg_context;

int Crypto_MBED_GenerateRandomData(const size_t inRandomDataLen, uint8_t * const outRandomData)
{
    int ret = -EINVAL;

    // validate input
    require(outRandomData, exit);

    int result = mbedtls_ctr_drbg_random(&host_drbg_context, outRandomData, inRandomDataLen);
    ret = (result == 0) ? 0 : -EINVAL;
    verify_noerr(ret);

exit:
    return ret;
}

#ifndef MBEDTLS_ENTROPY_C

#ifdef __ZEPHYR__
#include <zephyr/drivers/entropy.h>
static const struct device *entropy_dev;

static int _entropy_func(void *ctx, unsigned char *buf, size_t len)
{
    return entropy_get_entropy(entropy_dev, buf, len);
}
#else
static int _entropy_func(void *ctx, unsigned char *buf, size_t len)
{
    uint32_t now;
    uint32_t then;
    int delay;
    size_t i;

    then = 55; //TODO - get random value
    delay = then & 0xFF;
    while (delay > 0)
    {
        delay--;
    }

    now = 77; // TODO - get random value

    for (i = 0; i < len; i++)
    {
        buf[i] = (now >> i) & 0xFF;
    }
    for (; i < len; i++)
    {
        buf[i] = (then >> ((i + (then & 3)) & 3)) & 0xFF;
    }
    for (; i < len; i++)
    {
        buf[i] = ((now ^ then) >> (i & 3)) & 0xFF;
    }
    for (i = 0; i < len; i++)
    {
        buf[i] = ((now + then) >> ((now + i) & 3))& 0xFF;
    }
    *out_len = i;

    return 0;
}
#endif
#endif

int Crypto_MBED_RngInit(void)
{
    int ret = -EINVAL;
    const char identifier[] = "cryptoHW";

#ifdef MBEDTLS_ENTROPY_C
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_init(&host_drbg_context);
    int result = mbedtls_ctr_drbg_seed(
            &host_drbg_context,
            mbedtls_entropy_func,
            &entropy,
            (const unsigned char *) identifier,
            sizeof(identifier));

    if (result != 0)
    {
        // Handle error
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&host_drbg_context);
        goto exit;
    }
    else
    {
        ret = 0;
    }
#else
  #ifdef __ZEPHYR__
    entropy_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
    require(entropy_dev, exit);
  #endif
    mbedtls_ctr_drbg_init(&host_drbg_context);
    int result = mbedtls_ctr_drbg_seed(
                    &host_drbg_context,
                    _entropy_func,
                    NULL,
                    (const unsigned char *)identifier,
                    sizeof(identifier));
    if (result != 0)
    {
        // Handle error
        mbedtls_ctr_drbg_free(&host_drbg_context);
        goto exit;
    }
    else
    {
        ret = 0;
    }
#endif
exit:
    return ret;
}

#endif

