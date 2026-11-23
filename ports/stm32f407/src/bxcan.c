#include "bxcan.h"
#include "stm32f407_min.h"
#include <stdint.h>
#define RCC_APB1ENR REG32(0x40023840u)
#define GPIOB_MODER REG32(0x40020400u)
#define GPIOB_OSPEEDR REG32(0x40020408u)
#define GPIOB_PUPDR REG32(0x4002040Cu)
#define GPIOB_AFRH REG32(0x40020424u)
#define CB (0x40006400u)
#define CAN_MCR REG32(CB+0x00u)
#define CAN_MSR REG32(CB+0x04u)
#define CAN_TSR REG32(CB+0x08u)
#define CAN_RF0R REG32(CB+0x0Cu)
#define CAN_ESR REG32(CB+0x18u)
#define CAN_BTR REG32(CB+0x1Cu)
#define CAN_TI0R REG32(CB+0x180u)
#define CAN_TDT0R REG32(CB+0x184u)
#define CAN_TDL0R REG32(CB+0x188u)
#define CAN_TDH0R REG32(CB+0x18Cu)
#define CAN_RI0R REG32(CB+0x1B0u)
#define CAN_RDT0R REG32(CB+0x1B4u)
#define CAN_RDL0R REG32(CB+0x1B8u)
#define CAN_RDH0R REG32(CB+0x1BCu)
#define CAN_FMR REG32(CB+0x200u)
#define CAN_FM1R REG32(CB+0x204u)
#define CAN_FS1R REG32(CB+0x20Cu)
#define CAN_FFA1R REG32(CB+0x214u)
#define CAN_FA1R REG32(CB+0x21Cu)
#define CAN_F0R1 REG32(CB+0x240u)
#define CAN_F0R2 REG32(CB+0x244u)
#define TIM2_CR1 REG32(0x40000000u)
#define TIM2_CNT REG32(0x40000024u)
#define TIM2_PSC REG32(0x40000028u)
#define TIM2_ARR REG32(0x4000002Cu)
#define TIM2_EGR REG32(0x40000014u)
#define CAN_MCR_INRQ 1u
#define CAN_MCR_ABOM (1u<<6)
#define CAN_MSR_INAK 1u
#define CAN_TSR_TME0 (1u<<26)
#define CAN_TSR_RQCP0 (1u<<0)
#define CAN_TSR_TXOK0 (1u<<1)
#define CAN_ESR_BOFF (1u<<2)
#define CAN_RF0R_RFOM0 (1u<<5)
static uint8_t tx_pending;
static uint8_t tx_fault;
static int poll_bit(volatile uint32_t *reg, uint32_t bit, int want) {
    uint32_t limit=2000000u; /* finite loop, NOT a guaranteed time duration */
    while (limit--) {
        if (((*reg&bit)!=0u)==(want!=0)) return 0;
    }
    return -1;
}
int bxcan_init(void) {
    RCC_AHB1ENR |= 1u<<1; /* GPIOB enable */
    RCC_APB1ENR |= 1u<<25; /* CAN1 enable */
    (void)RCC_APB1ENR;
    GPIOB_MODER=(GPIOB_MODER & ~((3u<<16)|(3u<<18))) | (2u<<16)|(2u<<18);
    GPIOB_OSPEEDR |= (3u<<16)|(3u<<18);
    GPIOB_PUPDR=(GPIOB_PUPDR & ~((3u<<16)|(3u<<18))) | (1u<<16); /* PB8 pullup */
    GPIOB_AFRH=(GPIOB_AFRH & ~0xFFu) | 0x99u; /* PB8/PB9 AF9 */
    CAN_MCR = CAN_MCR_INRQ | CAN_MCR_ABOM;
    if (poll_bit((volatile uint32_t *)(uintptr_t)(CB+0x04u),CAN_MSR_INAK,1)!=0) return -1;
    /* APB1 16MHz; Prescaler=2, BS1=13tq, BS2=2tq, SJW=1 => 500kbps, 87.5% sample. */
    CAN_BTR=1u | (12u<<16) | (1u<<20);
    /* Filter bank0: 32-bit ID/mask, accept 0x7E0 standard data frames only. */
    CAN_FMR |= 1u;
    CAN_FA1R &= ~1u;
    CAN_FM1R &= ~1u;
    CAN_FS1R |= 1u;
    CAN_FFA1R &= ~1u;
    CAN_F0R1 = 0x7E0u << 21;
    CAN_F0R2 = (0x7FFu << 21) | (1u<<2) | (1u<<1);
    CAN_FA1R |= 1u;
    CAN_FMR &= ~1u;
    CAN_MCR &= ~CAN_MCR_INRQ;
    if (poll_bit((volatile uint32_t *)(uintptr_t)(CB+0x04u),CAN_MSR_INAK,0)!=0) return -1;
    tx_pending=0u;tx_fault=0u;
    return 0;
}
int bxcan_tx_complete(void) {
    uint32_t status;
    if (tx_fault || (CAN_ESR & CAN_ESR_BOFF)) return -1;
    if (!tx_pending) return 1;
    status=CAN_TSR;
    if ((status & CAN_TSR_RQCP0)==0u) return 0;
    tx_pending=0u;
    CAN_TSR=CAN_TSR_RQCP0; /* W1C clear mailbox0 completion flags */
    if (!(status & CAN_TSR_TXOK0)) { tx_fault=1u;return -1; }
    return 1;
}
int bxcan_send_response(void *ctx, const uint8_t frame[8]) {
    uint32_t lo,hi;
    (void)ctx;
    if (!frame || bxcan_tx_complete()!=1 || !(CAN_TSR & CAN_TSR_TME0)) return -1;
    lo=(uint32_t)frame[0]|((uint32_t)frame[1]<<8)|((uint32_t)frame[2]<<16)|((uint32_t)frame[3]<<24);
    hi=(uint32_t)frame[4]|((uint32_t)frame[5]<<8)|((uint32_t)frame[6]<<16)|((uint32_t)frame[7]<<24);
    CAN_TDT0R=8u;CAN_TDL0R=lo;CAN_TDH0R=hi;
    CAN_TI0R=(0x7E8u<<21)|1u;
    tx_pending=1u;
    return 0;
}
int bxcan_read(uint8_t frame[8]) {
    uint32_t lo,hi,id,rir,rdtr;
    if (!frame || !(CAN_RF0R & 3u)) return 0;
    rir=CAN_RI0R;rdtr=CAN_RDT0R;lo=CAN_RDL0R;hi=CAN_RDH0R;
    CAN_RF0R=CAN_RF0R_RFOM0; /* release FIFO mailbox */
    id=(rir>>21)&0x7FFu;
    if ((rir & 6u)!=0u || id!=0x7E0u || (rdtr & 0xFu)!=8u) return -1;
    for (unsigned i=0;i<4u;i++) {
        frame[i]=(uint8_t)(lo>>(8u*i));
        frame[4u+i]=(uint8_t)(hi>>(8u*i));
    }
    return 1;
}
void bxcan_time_init(void) {
    RCC_APB1ENR |= 1u<<0; /* TIM2 clock, APB1 16MHz */
    (void)RCC_APB1ENR;
    TIM2_CR1=0u;
    TIM2_PSC=15999u; /* 16 MHz / 16000 => 1kHz tick */
    TIM2_ARR=0xFFFFFFFFu;
    TIM2_EGR=1u;
    TIM2_CNT=0u;
    TIM2_CR1=1u;
}
uint32_t bxcan_now_ms(void) { return TIM2_CNT; }
void bxcan_system_reset(void) {
    stm_dsb();
    REG32(0xE000ED0Cu)=0x05FA0004u; /* SCB AIRCR VECTKEY + SYSRESETREQ */
    stm_dsb();
    for (;;) { __asm volatile("nop"); }
}
