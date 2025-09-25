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
    *(.isr_vector*)
    *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    /* Initialization/finalization arrays placed here to keep contiguous layout before data load */
    KEEP(*(.preinit_array*))
    KEEP(*(.init_array*))
    KEEP(*(.fini_array*))
    *(.ARM.extab*)
    *(.ARM.exidx*)
  } > FLASH

  /* Dummy legacy sections removed to prevent LMA overlap */

  _sidata = LOADADDR(.data);
  .data : AT (ADDR(.text) + SIZEOF(.text))
  {
    _sdata = .;
    *(.data*)
    *(.ramfunc*)
    _edata = .;
  } > RAM

  /* Place CCM data after BSS as NOLOAD so binary is not padded to CCM base by objcopy */
  /* CCM variables: place in CCMRAM but do not include in loadable image to avoid padding gap */
  /* To avoid objcopy padding the image up to 0x10000000 we place CCM symbols but discard contents in binary */
  .ccm_data (NOLOAD): { _sccm = .; *(.ccmram*) _eccm = .; } > CCMRAM

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
