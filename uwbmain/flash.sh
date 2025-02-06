#!/bin/sh


SCRIPT="$0"
PRODUCT="$(echo "$1" | tr '[:upper:]' '[:lower:]')" # force lowercase
REVISION="$(echo "$2" | tr '[:upper:]' '[:lower:]')" # force lowercase
if [ "$PRODUCT" = "" ] || [ "$REVISION" = "" ]; then
	echo "usage: $SCRIPT product revision\ne.g. $SCRIPT outer evt"
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
BOARD_NAME=${revision_name}
BOARD_ROOT=${PWD}/..

echo "flashing "build_${product_name}_${BOARD_NAME}/zephyr/merged.hex

nrfjprog -f nrf53  --coprocessor CP_APPLICATION --program build_${product_name}_${BOARD_NAME}/zephyr/merged.hex --sectorerase --verify --reset

