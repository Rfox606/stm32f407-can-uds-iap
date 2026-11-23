#ifndef STM32F407_MIN_H
#define STM32F407_MIN_H
#include <stdint.h>
/* Minimal register definitions for STM32F407VG (RM0090 + ARMv7-M).
 * Deliberately independent of HAL, only for the M1 boot path and flash driver.
 * M2 CAN initialization will be a separate peripheral module.
 */
#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define FLASH_BASE_REG 0x40023C00u
#define FLASH_KEYR REG32(FLASH_BASE_REG + 0x04u)
#define FLASH_SR   REG32(FLASH_BASE_REG + 0x0Cu)
#define FLASH_CR   REG32(FLASH_BASE_REG + 0x10u)
#define FLASH_ACR  REG32(FLASH_BASE_REG + 0x00u)
#define FLASH_SR_EOP      (1u << 0)
#define FLASH_SR_OPERR    (1u << 1)
#define FLASH_SR_WRPERR   (1u << 4)
#define FLASH_SR_PGAERR   (1u << 5)
#define FLASH_SR_PGPERR   (1u << 6)
#define FLASH_SR_PGSERR   (1u << 7)
#define FLASH_SR_BSY      (1u << 16)
#define FLASH_SR_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR)
#define FLASH_CR_PG       (1u << 0)
#define FLASH_CR_SER      (1u << 1)
#define FLASH_CR_SNB_Pos  3u
#define FLASH_CR_SNB_Msk  (15u << FLASH_CR_SNB_Pos)
#define FLASH_CR_PSIZE_Msk (3u << 8)
#define FLASH_CR_PSIZE_X32 (2u << 8)
#define FLASH_CR_STRT     (1u << 16)
#define FLASH_CR_LOCK     (1u << 31)
#define RCC_AHB1ENR REG32(0x40023830u)
#define GPIOA_IDR   REG32(0x40020010u)
#define GPIOD_MODER REG32(0x40020C00u)
#define GPIOD_BSRR  REG32(0x40020C18u)
#define SCB_ICSR    REG32(0xE000ED04u)
#define SCB_VTOR    REG32(0xE000ED08u)
#define SCB_SHCSR   REG32(0xE000ED24u)
#define SCB_CPACR   REG32(0xE000ED88u)
#define SYST_CSR    REG32(0xE000E010u)
#define SYST_RVR    REG32(0xE000E014u)
#define SYST_CVR    REG32(0xE000E018u)
#define NVIC_ICER(n) REG32(0xE000E180u + 4u * (n))
#define NVIC_ICPR(n) REG32(0xE000E280u + 4u * (n))
#define SCB_ICSR_PENDSTCLR (1u << 25)
#define SCB_ICSR_PENDSVCLR (1u << 27)
static inline void stm_dsb(void) { __asm volatile ("dsb 0xF" ::: "memory"); }
static inline void stm_isb(void) { __asm volatile ("isb 0xF" ::: "memory"); }
static inline void stm_irq_disable(void) { __asm volatile ("cpsid i" ::: "memory"); }
static inline void stm_irq_enable(void) { __asm volatile ("cpsie i" ::: "memory"); }
#endif
