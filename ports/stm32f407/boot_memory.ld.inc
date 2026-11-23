/* TEMPLATE ONLY: paste MEMORY section into an STM32F407 bootloader linker script. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}
