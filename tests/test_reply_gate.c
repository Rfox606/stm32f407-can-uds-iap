/* M4: deterministic software injection of CAN mailbox busy / send failure /
 * TX not yet acknowledged. This is NOT a physical bxCAN timing test.
 */
#include "reply_gate.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct { int fail_next; int sent; uint8_t last[8]; } tx_record_t;
static int fake_tx(void *ctx,const uint8_t frame[8]) {
    tx_record_t *r=(tx_record_t *)ctx;
    if (r->fail_next) {r->fail_next=0;return -1;}
    ++r->sent;
    memcpy(r->last,frame,8);
    return 0;
}
static int pending_response_guard(void) {
    tx_record_t record={0};
    isotp_t tp;
    reply_gate_t gate;
    const uint8_t pending[]={0x7F,0x34,0x78};
    int erase_steps=0;
    isotp_init(&tp,fake_tx,&record);
    reply_gate_init(&gate);
    CHECK(reply_gate_stage(&gate,pending,sizeof(pending))==0);
    CHECK(reply_gate_stage(&gate,pending,sizeof(pending))==-1);
    reply_gate_poll(&gate,&tp,0,10); /* hardware mailbox busy */
    CHECK(record.sent==0 && !reply_gate_take_ack(&gate));
    reply_gate_poll(&gate,&tp,-1,11); /* bus-off / CAN send error */
    CHECK(record.sent==0 && !reply_gate_take_ack(&gate));
    record.fail_next=1;
    reply_gate_poll(&gate,&tp,1,12); /* accepted mailbox, send callback fails */
    CHECK(record.sent==0 && reply_gate_pending(&gate));
    reply_gate_poll(&gate,&tp,1,13); /* send now accepted */
    CHECK(record.sent==1 && record.last[0]==3 && record.last[1]==0x7F);
    CHECK(!reply_gate_take_ack(&gate));
    reply_gate_poll(&gate,&tp,0,14); /* transmitted, awaiting hardware TXOK */
    CHECK(reply_gate_pending(&gate) && !reply_gate_take_ack(&gate));
    reply_gate_poll(&gate,&tp,1,15); /* actual TXOK */
    if(reply_gate_take_ack(&gate)) ++erase_steps;
    CHECK(erase_steps==1 && !reply_gate_pending(&gate));
    CHECK(!reply_gate_take_ack(&gate)); /* ACK is consumed once */

    /* Following Flash operation emits another ResponsePending. Neither the
     * next erase nor an APP handoff may run before this next TXOK. */
    CHECK(reply_gate_stage(&gate,pending,sizeof(pending))==0);
    reply_gate_poll(&gate,&tp,1,16);
    CHECK(record.sent==2 && !reply_gate_take_ack(&gate));
    reply_gate_poll(&gate,&tp,1,17);
    CHECK(reply_gate_take_ack(&gate) && !reply_gate_take_ack(&gate));
    return 0;
}
static int app_handoff_guard(void) {
    tx_record_t record={0};
    isotp_t tp;
    reply_gate_t gate;
    const uint8_t approved[]={0x50,0x02,0,50,1,0xF4};
    int software_reset=0;
    isotp_init(&tp,fake_tx,&record);
    reply_gate_init(&gate);
    CHECK(reply_gate_stage(&gate,approved,sizeof(approved))==0);
    for(int t=0;t<5;t++) {
        reply_gate_poll(&gate,&tp,0,(uint32_t)t);
        if(reply_gate_take_ack(&gate)) ++software_reset;
    }
    CHECK(software_reset==0 && record.sent==0);
    reply_gate_poll(&gate,&tp,1,6);
    CHECK(record.sent==1 && software_reset==0);
    reply_gate_poll(&gate,&tp,-1,7);
    CHECK(software_reset==0);
    reply_gate_poll(&gate,&tp,1,8);
    if(reply_gate_take_ack(&gate)) ++software_reset;
    CHECK(software_reset==1);
    return 0;
}
static int invalid_and_safe_retry(void) {
    reply_gate_t gate;
    const uint8_t data[8]={1,2,3,4,5,6,7,8};
    reply_gate_init(&gate);
    CHECK(reply_gate_stage(&gate,0,1)==-1);
    CHECK(reply_gate_stage(&gate,data,0)==-1);
    CHECK(reply_gate_stage(&gate,data,8)==-1);
    CHECK(!reply_gate_pending(&gate) && !reply_gate_take_ack(&gate));
    return 0;
}
int main(void) {
    CHECK(pending_response_guard()==0);
    CHECK(app_handoff_guard()==0);
    CHECK(invalid_and_safe_retry()==0);
    puts("3/3 M4 CAN response gate scenarios passed (software only)");
    return 0;
}
