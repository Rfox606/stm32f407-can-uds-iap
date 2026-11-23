/* M3 software integration bridge: Python sends complete UDS payloads to
 * the actual C UDS firmware implementation (not CAN, not STM32 emulation).
 * All Flash operations use the existing host mock_flash. Output is one
 * R <hex> line per UDS response; a controlled dropped ACK emits TIMEOUT.
 */
#include "uds_boot.h"
#include "mock_flash.h"
#include "iap/config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static mock_flash_t memory;
static uds_boot_t server;
static uint8_t drop36_once;
static uint8_t corrupt_at_finish;
static uint8_t reset_after_second_block;
static uint8_t dropped;
static unsigned accepted_blocks;
static uint32_t logical_ms=10u;

static int nibble(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
static int decode_hex(const char *line, uint8_t *out, size_t max) {
    size_t count=0;
    while (*line) {
        int a,b;
        if (*line=='\n' || *line=='\r') break;
        if (count>=max || !line[1]) return -1;
        a=nibble(line[0]);b=nibble(line[1]);
        if (a<0 || b<0) return -1;
        out[count++]=(uint8_t)((a<<4)|b);
        line+=2;
    }
    return (int)count;
}
static void respond(const uint8_t *data,size_t n) {
    if (!n) return;
    if (drop36_once && !dropped && n==2u && data[0]==0x76u) {
        dropped=1;
        fputs("TIMEOUT\n",stdout);
        fflush(stdout);
        return;
    }
    fputs("R ",stdout);
    for(size_t i=0;i<n;i++) printf("%02X",data[i]);
    fputc('\n',stdout);
    fflush(stdout);
}
int main(int argc,char **argv) {
    char line[1024];
    uint8_t q[320],response[8];
    size_t n;
    for(int i=1;i<argc;i++) {
        if(strcmp(argv[i],"--drop-36-once")==0) drop36_once=1u;
        else if(strcmp(argv[i],"--corrupt-before-finish")==0) corrupt_at_finish=1u;
        else if(strcmp(argv[i],"--reset-after-second-block")==0) reset_after_second_block=1u;
        else {fputs("unknown bridge option\n",stderr);return 2;}
    }
    mock_flash_reset(&memory);
    uds_boot_init(&server,mock_flash_ops(&memory),logical_ms);
    while(fgets(line,sizeof(line),stdin)) {
        if(strcmp(line,"QUIT\n")==0) return 0;
        if(strcmp(line,"RESET\n")==0) {
            uds_boot_init(&server,mock_flash_ops(&memory),++logical_ms);
            fputs("OK\n",stdout);fflush(stdout);continue;
        }
        if(strncmp(line,"S ",2)!=0) {
            fputs("INVALID\n",stdout);fflush(stdout);continue;
        }
        int qlen=decode_hex(line+2,q,sizeof(q));
        if(qlen<=0) {fputs("INVALID\n",stdout);fflush(stdout);continue;}
        if(corrupt_at_finish && q[0]==0x37u && server.updater.written>128u) {
            size_t index=(size_t)(IAP_APP_BASE-IAP_FLASH_BASE)+100u;
            memory.data[index]^=1u; /* actual C updater must detect CRC mismatch */
            corrupt_at_finish=0u;
        }
        if(uds_boot_request(&server,q,(size_t)qlen,response,&n,++logical_ms)!=0) {
            fputs("INVALID\n",stdout);fflush(stdout);continue;
        }
        respond(response,n);
        while(server.busy_erase) {
            if(uds_boot_erase_step(&server,response,&n,++logical_ms)>0) respond(response,n);
        }
        while(server.busy_finish) {
            if(uds_boot_finish_step(&server,response,&n,++logical_ms)>0) respond(response,n);
        }
        while(server.busy_verify) {
            if(uds_boot_verify_step(&server,response,&n,++logical_ms)>0) respond(response,n);
        }
        if(q[0]==0x36u && n>0u && response[0]==0x76u) ++accepted_blocks;
        if(reset_after_second_block && accepted_blocks==2u && q[0]==0x36u) {
            /* Model only volatile UDS state loss; Flash content is unchanged. */
            uds_boot_init(&server,mock_flash_ops(&memory),++logical_ms);
            reset_after_second_block=0u;
        }
    }
    return 0;
}
