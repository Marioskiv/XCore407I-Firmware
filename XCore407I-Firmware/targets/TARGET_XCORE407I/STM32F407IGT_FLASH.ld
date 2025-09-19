/* Linker script for STM32F407IGT6 (1MB Flash, 192KB RAM: 128KB SRAM + 64KB CCM) */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  CCMRAM (xrw)    : ORIGIN = 0x10000000, LENGTH = 64K    /* Core Coupled Memory */
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 128K   /* SRAM1 + SRAM2 */
}

/* Common stack/heap sizes (adjust as needed) */
_estack = ORIGIN(RAM) + LENGTH(RAM); /* Top of main SRAM */

/* Sections */
SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    /* ARM exception tables inside .text to keep contiguous and avoid LMA overlap */
    *(.ARM.extab*)
    *(.ARM.exidx*)
  } > FLASH

  /* Legacy names still provided (empty if already absorbed) */
  .ARM.extab : { *(.discard_arm_extab_dummy*) } > FLASH
  .ARM.exidx : { *(.discard_arm_exidx_dummy*) } > FLASH
  .preinit_array     : { KEEP (*(.preinit_array*)) } > FLASH
  .init_array        : { KEEP (*(.init_array*)) } > FLASH
  .fini_array        : { KEEP (*(.fini_array*)) } > FLASH

  _sidata = LOADADDR(.data);
  .data : AT (ADDR(.text) + SIZEOF(.text))
  {
    _sdata = .;
    *(.data*)
    *(.ramfunc*)
    _edata = .;
  } > RAM

  .ccm_data :
  {
    _sccm = .;
    *(.ccmram*)
    _eccm = .;
  } > CCMRAM

  .bss (NOLOAD):
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM

  ._user_heap_stack (NOLOAD):
  {
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + 0x1000; /* heap 4KB placeholder */
    . = . + 0x0800; /* stack 2KB placeholder (actual top uses _estack) */
  } > RAM
}
