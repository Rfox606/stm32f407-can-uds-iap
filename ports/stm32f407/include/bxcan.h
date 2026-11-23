#ifndef STM32F407_BXCAN_H
#define STM32F407_BXCAN_H
#include <stdint.h>
/* STM32F407 CAN1: PB8=RX, PB9=TX, AF9, external 3.3V CAN transceiver mandatory.
 * Classic CAN standard 11-bit, 500 kbit/s from APB1=16 MHz HSI (M1 clock).
 * Polling driver: do not rely on CAN IRQs surviving single-bank Flash erase.
 */
int bxcan_init(void);
int bxcan_read(uint8_t frame[8], uint8_t *dlc);
int bxcan_send_response(void *ctx, const uint8_t frame[8]);
/* 1 = last frame acknowledged; 0 = hardware still sending; -1 = TX error/bus-off */
int bxcan_tx_complete(void);
void bxcan_time_init(void);
uint32_t bxcan_now_ms(void);
void bxcan_system_reset(void) __attribute__((noreturn));
#endif
