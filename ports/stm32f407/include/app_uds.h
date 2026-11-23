#ifndef IAP_APP_UDS_H
#define IAP_APP_UDS_H
#include <stddef.h>
#include <stdint.h>
/* App-only, minimal UDS services: 22 F190, 22 F192, 10 02, 3E 00.
 * Normal app never writes Flash; it only requests a software handoff.
 */
int app_uds_request(const uint8_t *req, size_t len, uint8_t out[8], size_t *out_len);
int app_uds_handoff_pending(void);
#endif
