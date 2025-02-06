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

int Crypto_PSA_GenerateRandomData(const size_t inRandomDataLen, uint8_t * const outRandomData);

int Crypto_PSA_RngInit(void);

