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

#include <mbedtls/ctr_drbg.h>

#include <stdint.h>
#include <stddef.h>

extern mbedtls_ctr_drbg_context host_drbg_context;

int Crypto_MBED_GenerateRandomData(const size_t inRandomDataLen, uint8_t * const outRandomData);

int Crypto_MBED_RngInit(void);

