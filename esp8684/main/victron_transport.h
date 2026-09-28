#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Session-only credit accounting. Callers serialize access with the RX lock.
 * F9 grants additional chunks, not an absolute counter. Return exactly the
 * consumed RX credits; granting 65 after every 32 chunks grows the window.
 */
typedef struct {
    uint16_t tx, received;
    uint8_t tx_limit;
    bool failed;
} victron_flow_t;

static inline bool victron_flow_init(victron_flow_t *f,const uint8_t *p,size_t n) {
    f->tx=0;f->received=0;f->failed=true;
    /* Control info: protocol byte, version u16, maximum TX chunks, maximum
     * CBOR size, ATT chunk size u16. Wait for the initial F9 grant; the maximum
     * is not itself a grant and must not be counted twice.
     */
    if(n<7 || p[0]!=0 || !p[3] || p[4]<8) return false;
    f->tx_limit=p[3];f->failed=false;return true;
}

static inline bool victron_flow_control(victron_flow_t *f,const uint8_t *p,size_t n) {
    if(!n) { f->failed=true; return false; }
    if(p[0]==0xf7) f->failed=true;
    if(p[0]==0xf9) {
        if(n!=2 || f->tx+p[1]>f->tx_limit) f->failed=true;
        else f->tx+=p[1];
    }
    return !f->failed;
}
static inline bool victron_flow_take_tx(victron_flow_t *f) {
    if(f->failed || !f->tx) return false;
    --f->tx; return true;
}
static inline void victron_flow_received(victron_flow_t *f) {
    if(f->received==128) f->failed=true;
    else ++f->received;
}
static inline uint8_t victron_flow_return_rx(victron_flow_t *f) {
    if(f->failed || f->received<32) return 0;
    uint8_t n=(uint8_t)f->received; f->received=0; return n;
}
/* Keep each request within the default ATT_MTU(23)-3, independently of MTU
 * negotiation. Current requests are at most 8 bytes. Never truncate CBOR.
 */
static inline bool victron_request_fits(size_t n) { return n>0 && n<=20; }
static inline int victron_mppt_expected_output(uint8_t mode) {
    return (mode&15)==4?1:(mode&15)==0?0:-1;
}
