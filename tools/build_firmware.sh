#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-arm}"
mkdir -p "$BUILD_DIR"
CC="${ARM_CC:-arm-none-eabi-gcc}"
OBJCOPY="${ARM_OBJCOPY:-arm-none-eabi-objcopy}"
OBJDUMP="${ARM_OBJDUMP:-arm-none-eabi-objdump}"
COMMON=(-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-common -fdata-sections -ffunction-sections -Os -g3 -std=c11 -Wall -Wextra -Werror -Wpedantic -I"${ROOT}/include" -I"${ROOT}/ports/stm32f407/include" -I"${ROOT}/protocol/include")
LINK=(-nostdlib -Wl,--gc-sections)
if [[ "$CC" == *clang* ]]; then
  COMMON=(--target=arm-none-eabi "${COMMON[@]}")
  LINK+=(-fuse-ld=lld)
fi
SHARED=("${ROOT}/ports/stm32f407/src/startup_stm32f407.S" \
        "${ROOT}/ports/stm32f407/src/clock.c" \
        "${ROOT}/ports/stm32f407/src/board.c" \
        "${ROOT}/ports/stm32f407/src/runtime_mem.c" \
        "${ROOT}/ports/stm32f407/src/boot_handoff.c")
for TARGET in boot app; do
  SOURCES=("${SHARED[@]}")
  if [[ "$TARGET" == boot ]]; then
    SOURCES+=("${ROOT}/ports/stm32f407/src/boot_main.c" \
      "${ROOT}/ports/stm32f407/src/boot_jump.c" \
      "${ROOT}/ports/stm32f407/src/flash_layout.c" \
      "${ROOT}/ports/stm32f407/src/flash_stm32f407.c" \
      "${ROOT}/src/crc32.c" "${ROOT}/src/image.c" "${ROOT}/src/updater.c" \
      "${ROOT}/protocol/src/isotp.c" "${ROOT}/protocol/src/uds_boot.c" \
      "${ROOT}/protocol/src/reply_gate.c" \
      "${ROOT}/ports/stm32f407/src/bxcan.c")
  else
    SOURCES+=("${ROOT}/ports/stm32f407/src/app_main.c" \
      "${ROOT}/ports/stm32f407/src/app_uds.c" \
      "${ROOT}/ports/stm32f407/src/bxcan.c" \
      "${ROOT}/protocol/src/isotp.c" \
      "${ROOT}/protocol/src/reply_gate.c" \
      "${ROOT}/src/crc32.c")
  fi
  echo "==> Building ${TARGET} with ${CC}"
  "$CC" "${COMMON[@]}" "${SOURCES[@]}" "${LINK[@]}" \
    -Wl,-T,"${ROOT}/ports/stm32f407/linker/${TARGET}.ld" \
    -Wl,-Map,"${BUILD_DIR}/${TARGET}.map" \
    -o "${BUILD_DIR}/${TARGET}.elf"
  "$OBJCOPY" -O binary "${BUILD_DIR}/${TARGET}.elf" "${BUILD_DIR}/${TARGET}.bin"
  "$OBJDUMP" -d "${BUILD_DIR}/${TARGET}.elf" > "${BUILD_DIR}/${TARGET}.disasm.txt"
  ls -l "${BUILD_DIR}/${TARGET}.elf" "${BUILD_DIR}/${TARGET}.bin"
  if command -v arm-none-eabi-size >/dev/null 2>&1; then
    arm-none-eabi-size "${BUILD_DIR}/${TARGET}.elf"
  fi
done
python3 "${ROOT}/tools/check_arm_images.py" "$BUILD_DIR"
python3 "${ROOT}/tools/make_factory_metadata.py" "$BUILD_DIR/app.bin" "$BUILD_DIR/app_factory_meta.bin"
PYTHONPATH="${ROOT}" python3 -m host.flash_cli inspect "$BUILD_DIR/app.bin" --version 0x010000
