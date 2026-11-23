#ifndef IAP_CONFIG_H
#define IAP_CONFIG_H

/* STM32F407VGT6, 1 MiB Flash. See ST RM0090 sector map. */
#define IAP_FLASH_BASE          0x08000000u
#define IAP_FLASH_BYTES         0x00100000u
#define IAP_BOOT_BASE           0x08000000u
#define IAP_BOOT_BYTES          0x00010000u /* Sectors 0..3, 64 KiB */
#define IAP_META_BASE           0x08010000u
#define IAP_META_BYTES          0x00010000u /* Sector 4, 64 KiB */
#define IAP_APP_BASE            0x08020000u
#define IAP_APP_BYTES           0x000E0000u /* Sectors 5..11, 896 KiB */
#define IAP_IMAGE_MAGIC         0x49504131u /* "IAP1" in little-endian */
#define IAP_MAX_BLOCK           256u
#define IAP_SRAM_BASE           0x20000000u
#define IAP_SRAM_END            0x20020000u /* 128 KiB addressable SRAM1+2 */

#endif
