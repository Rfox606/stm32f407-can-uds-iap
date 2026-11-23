#ifndef IAP_IMAGE_H
#define IAP_IMAGE_H
#include "iap/flash_if.h"
#include <stdint.h>

typedef struct {
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t version;
    uint32_t record_crc32; /* CRC over preceding 12 bytes */
    uint32_t magic;        /* programmed LAST: final commit marker */
} iap_image_record_t;

typedef enum { IAP_BOOT_RECOVERY = 0, IAP_BOOT_APP = 1 } iap_boot_choice_t;
int iap_image_validate(const iap_flash_t *flash, iap_image_record_t *out_record);
iap_boot_choice_t iap_boot_select(const iap_flash_t *flash);
#endif
