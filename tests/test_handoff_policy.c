#include "boot_handoff.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d\n",__LINE__); return 1; } } while(0)
int main(void) {
    CHECK(BOOT_HANDOFF_ADDRESS >= BOOT_INITIAL_MSP+16u);
    CHECK(BOOT_HANDOFF_ADDRESS+8u <= 0x20020000u);
    CHECK(!boot_handoff_marker_valid(0,BOOT_HANDOFF_MAGIC,~BOOT_HANDOFF_MAGIC));
    CHECK(boot_handoff_marker_valid(1u<<28,BOOT_HANDOFF_MAGIC,~BOOT_HANDOFF_MAGIC));
    CHECK(!boot_handoff_marker_valid(1u<<28,BOOT_HANDOFF_MAGIC,0u));
    CHECK(!boot_handoff_marker_valid(1u<<28,0u,~BOOT_HANDOFF_MAGIC));
    CHECK(!boot_handoff_marker_valid(1u<<27,BOOT_HANDOFF_MAGIC,~BOOT_HANDOFF_MAGIC));
    puts("7/7 mailbox address and reset-origin checks passed (pure logic)");
    return 0;
}
