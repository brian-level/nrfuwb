#!/bin/sh
REL_BASE=../../../mead_ncs/zephyr
cd ${REL_BASE}
export ZEPHYR_BASE=${PWD}
cd ${OLDPWD}
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GCC_CROSS_VERSION=12.2.rel1
export GNU_INSTALL_ROOT=/usr/local/GNU-Tools-ARM-Embedded/${GCC_CROSS_VERSION}
export GNUARMEMB_TOOLCHAIN_PATH=${GNU_INSTALL_ROOT}/
