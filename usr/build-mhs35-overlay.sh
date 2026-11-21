#!/bin/bash
# Rebuild usr/mhs35-overlay.dtb from usr/mhs35-overlay.dts.
# The upstream repo shipped only the binary; the source now lives
# alongside it so the overlay can be reviewed and rebuilt.
# Needs: dtc (device-tree-compiler) and a C preprocessor (gcc).
set -eu
cd "$(dirname "$0")"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# The overlay only needs the two GPIO polarity constants from
# dt-bindings/gpio/gpio.h. Use the real kernel headers if present,
# otherwise a local stub with the same values.
INC="$WORK/include"
mkdir -p "$INC/dt-bindings/gpio"
if [ -f /usr/src/linux-headers-*/include/dt-bindings/gpio/gpio.h ]; then
  INC=/usr/src/linux-headers-*/include
  INC=$(echo $INC | awk '{print $1}')
else
  printf '#define GPIO_ACTIVE_HIGH 0\n#define GPIO_ACTIVE_LOW 1\n' \
    > "$INC/dt-bindings/gpio/gpio.h"
fi

gcc -E -nostdinc -I"$INC" -undef -D__DTS__ -x assembler-with-cpp \
  -o "$WORK/mhs35-pp.dts" mhs35-overlay.dts
# no-unit_address_vs_reg: the spidev@0/@1 nodes are status-only
# overrides of nodes that carry reg in the base tree (same shape
# as the originally shipped binary).
dtc -@ -W no-unit_address_vs_reg -I dts -O dtb \
  -o mhs35-overlay.dtb "$WORK/mhs35-pp.dts"
echo "built mhs35-overlay.dtb ($(stat -c%s mhs35-overlay.dtb) bytes)"
