#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-arm}"
mkdir -p "$BUILD_DIR"
CC="${ARM_CC:-arm-none-eabi-gcc}"
OBJCOPY="${ARM_OBJCOPY:-arm-none-eabi-objcopy}"
OBJDUMP="${ARM_OBJDUMP:-arm-none-eabi-objdump}"
COMMON=(-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-common -fdata-sections -ffunction-sections -Os -g3 -std=c11 -Wall -Wextra -Werror -Wpedantic -I"${ROOT}/include" -I"${ROOT}/ports/stm32f407/include")
LINK=(-nostdlib -Wl,--gc-sections)
if [[ "$CC" == *clang* ]]; then
  COMMON=(--target=arm-none-eabi "${COMMON[@]}")
  LINK+=(-fuse-ld=lld)
fi
SHARED=("${ROOT}/ports/stm32f407/src/startup_stm32f407.S" \
        "${ROOT}/ports/stm32f407/src/clock.c" \
        "${ROOT}/ports/stm32f407/src/board.c" \
        "${ROOT}/ports/stm32f407/src/runtime_mem.c")
for TARGET in boot app; do
  SOURCES=("${SHARED[@]}")
  if [[ "$TARGET" == boot ]]; then
    SOURCES+=("${ROOT}/ports/stm32f407/src/boot_main.c" \
      "${ROOT}/ports/stm32f407/src/boot_jump.c" \
      "${ROOT}/ports/stm32f407/src/flash_layout.c" \
      "${ROOT}/ports/stm32f407/src/flash_stm32f407.c" \
      "${ROOT}/src/crc32.c" "${ROOT}/src/image.c" "${ROOT}/src/updater.c")
  else
    SOURCES+=("${ROOT}/ports/stm32f407/src/app_main.c")
  fi
  echo "==> Building ${TARGET} with ${CC}"
  "$CC" "${COMMON[@]}" "${SOURCES[@]}" "${LINK[@]}" \
    -Wl,-T,"${ROOT}/ports/stm32f407/linker/${TARGET}.ld" \
    -Wl,-Map,"${BUILD_DIR}/${TARGET}.map" \
    -o "${BUILD_DIR}/${TARGET}.elf"
  "$OBJCOPY" -O binary "${BUILD_DIR}/${TARGET}.elf" "${BUILD_DIR}/${TARGET}.bin"
  "$OBJDUMP" -d "${BUILD_DIR}/${TARGET}.elf" > "${BUILD_DIR}/${TARGET}.disasm.txt"
  ls -l "${BUILD_DIR}/${TARGET}.elf" "${BUILD_DIR}/${TARGET}.bin"
done
python3 "${ROOT}/tools/check_arm_images.py" "$BUILD_DIR"
python3 "${ROOT}/tools/make_factory_metadata.py" "$BUILD_DIR/app.bin" "$BUILD_DIR/app_factory_meta.bin"
