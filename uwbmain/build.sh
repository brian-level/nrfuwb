#!/bin/sh

SCRIPT="$0"
PRODUCT="$(echo "$1" | tr '[:upper:]' '[:lower:]')" # force lowercase
REVISION="$(echo "$2" | tr '[:upper:]' '[:lower:]')" # force lowercase
if [ "$PRODUCT" = "" ] || [ "$REVISION" = "" ]; then
	echo "usage: $SCRIPT product revision\ne.g. $SCRIPT nrfuwb 5340dk"
	exit 1
fi
product_name=""
product_short=""
case $PRODUCT in
"nrfuwb")
	product_name="nrfuwb"
	product_short="uwb"
	;;
*)
	echo "unknown product $PRODUCT, should be nrfuwb"
	exit 1
esac
revision_name=""
case $REVISION in
"evt"|"dvt")
	revision_name="nrfuwb_$REVISION"_cpuapp
	;;
"nrf5340dk_nrf5340_cpuapp"|"5340dk"|"5340")
	revision_name="nrf5340dk_nrf5340_cpuapp"
	;;
*)
	echo "unknown revision, should be 5340dk | evt | dvt"
	exit 1
esac

# pull version from Jenkins if defined
if [ -f ../../build/build_version.txt ]; then
    VERSION=`head -n1 ../../build/build_version.txt`
    BUILD_SRC="Jenkins"
else
    if [ -f app_version.txt ]; then
        VERSION=`head -n1 app_version.txt`
    else
        VERSION="1.0.0+0"
    fi
    BUILD_SRC="Develop"
fi

GIT_HASH=`git rev-parse HEAD`
EXTRA_CFLAGS="-DGIT_HASH=\\\"${GIT_HASH}\\\" -DBUILD_SRC=\\\"${BUILD_SRC}\\\" ${EXTRA_CFLAGS_CI-} ${EXTRA_CFLAGS_MBEDTLS}"

BOARD_NAME=${revision_name}
BOARD_ROOT=${PWD}/..

echo "Building NRFUWB ${BOARD_NAME} ver ${VERSION}"
west build -b ${BOARD_NAME} -d build_${product_name}_${BOARD_NAME} -- -DBOARD_ROOT=$BOARD_ROOT \
    -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"${VERSION}\" \
	-DCMAKE_C_FLAGS="${EXTRA_CFLAGS}"

