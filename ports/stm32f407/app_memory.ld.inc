/* TEMPLATE ONLY: paste MEMORY section into an STM32F407 application linker script. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08020000, LENGTH = 896K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}
