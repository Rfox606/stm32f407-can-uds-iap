/* STM32F407VG: APP starts sector 5 (0x08020000), 896KiB available. */
/* Last 64 bytes of standard SRAM are reserved for APP->Bootloader mailbox.
 * Both firmwares must use this same reduced stack/memory region. */
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx)  : ORIGIN = 0x08020000, LENGTH = 896K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K - 64
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
  .app_info : { . = ALIGN(4); KEEP(*(.app_info)) . = ALIGN(4); } > FLASH
  .text : { . = ALIGN(4); *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH
  .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  . = ALIGN(8);
  _end = .;
  /DISCARD/ : { *(.comment*) *(.note*) }
}
ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(FLASH) + LENGTH(FLASH), "APP FLASH overflow")
ASSERT(_end <= ORIGIN(RAM) + LENGTH(RAM), "APP RAM overflow")
