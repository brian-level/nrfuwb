cmake_minimum_required(VERSION 3.20.0)
    target_sources(app PRIVATE
         crypto_mbed_chacha20.c
         crypto_mbed_ecc.c
         crypto_mbed_rng.c
         crypto_platform.c
         crypto_psa_chacha20.c
         crypto_psa_ecc.c
         crypto_psa_rng.c
#         crypto_secpk1_ecc.c
         micro_ecc.c
)

