#include "stm32f407_flash.h"
#include "iap/config.h"
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr);return 1; \
} } while (0)

int main(void) {
    stm32_flash_sector_t s;
    CHECK(stm32f407_sector_info(0u,&s)==0 && s.start==0x08000000u && s.size==0x4000u);
    CHECK(stm32f407_sector_info(3u,&s)==0 && s.start==0x0800C000u && s.size==0x4000u);
    CHECK(stm32f407_sector_info(4u,&s)==0 && s.start==0x08010000u && s.size==0x10000u);
    CHECK(stm32f407_sector_info(5u,&s)==0 && s.start==IAP_APP_BASE && s.size==0x20000u);
    CHECK(stm32f407_sector_info(11u,&s)==0 && s.start==0x080E0000u && s.size==0x20000u);
    CHECK(stm32f407_sector_info(12u,&s)!=0);
    CHECK(stm32f407_sector_info(1u,0)!=0);
    CHECK(!stm32f407_writable_range(IAP_BOOT_BASE,4));
    CHECK(!stm32f407_writable_range(IAP_BOOT_BASE+IAP_BOOT_BYTES-4u,8));
    CHECK(stm32f407_writable_range(IAP_META_BASE,20));
    CHECK(stm32f407_writable_range(IAP_APP_BASE,4));
    CHECK(stm32f407_writable_range(0x080FFFFCu,4));
    CHECK(!stm32f407_writable_range(0x080FFFFCu,8));
    CHECK(!stm32f407_writable_range(0xFFFFFFFFu,8));
    CHECK(!stm32f407_writable_range(IAP_APP_BASE,0));
    printf("15/15 STM32F407 Flash layout/boundary checks passed (host-side)\n");
    return 0;
}
