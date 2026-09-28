#include <assert.h>
#include <stdio.h>
#include "../../esp8684/main/wifi_client_policy.h"
int main(void) {
    wifi_client_policy_t s;
    wifi_client_begin(&s,0,0,90,60);
    assert(s.ap && s.phase==WIFI_PHASE_AP);
    assert(!wifi_client_step(&s,999999,false,true,0,false));
    wifi_client_begin(&s,1,1000,90,60);
    assert(!s.ap && wifi_client_next_seconds(&s,1000)==90);
    assert(!wifi_client_step(&s,90999,false,false,0,false));
    assert(!s.ap);
    assert(wifi_client_step(&s,91000,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(s.phase==WIFI_PHASE_CONNECTING && !s.ap);
    /* Association alone is not connected: only GOT_IP counts. */
    assert(!wifi_client_step(&s,92000,false,false,0,false));
    assert(!wifi_client_step(&s,150999,false,false,0,false));
    assert(!s.ap);
    assert(wifi_client_step(&s,151000,false,false,0,false)==(WIFI_CLIENT_DISCONNECT|WIFI_CLIENT_AP_ON));
    assert(s.phase==WIFI_PHASE_FALLBACK && s.ap);
    s.ap=false; /* esp_wifi_set_mode(APSTA) failed; retry next tick. */
    assert(wifi_client_step(&s,151001,false,false,0,false)==WIFI_CLIENT_AP_ON);
    assert(s.ap);
    assert(!wifi_client_step(&s,210999,false,true,0,false));
    assert(wifi_client_step(&s,211000,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(s.ap); /* Keep fallback while retrying the router. */
    s.ap=false;
    assert(wifi_client_step(&s,211001,false,false,0,false)==WIFI_CLIENT_AP_ON);
    assert(wifi_client_step(&s,212000,true,false,2,false)==WIFI_CLIENT_AP_OFF);
    assert(s.phase==WIFI_PHASE_CONNECTED && !s.ap);
    assert(!wifi_client_step(&s,213000,true,false,0,false));
    /* Router or DHCP lease loss restores AP; renewed DHCP retires it. */
    assert(wifi_client_step(&s,214000,false,true,0,false)==WIFI_CLIENT_AP_ON);
    assert(s.phase==WIFI_PHASE_FALLBACK && s.ap);
    assert(wifi_client_step(&s,219000,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(wifi_client_step(&s,220000,true,false,1,false)==WIFI_CLIENT_AP_OFF);
    assert(!s.ap);
    wifi_client_begin(&s,1,0,0,60);
    assert(wifi_client_step(&s,0,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(!wifi_client_step(&s,1000,true,false,0,false));
    assert(s.phase==WIFI_PHASE_CONNECTED && !s.ap); /* Never started AP. */
    wifi_client_begin(&s,1,0,0,10);
    assert(wifi_client_step(&s,0,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(!wifi_client_step(&s,1000,false,true,0,false));
    assert(!wifi_client_step(&s,5999,false,false,0,false));
    assert(wifi_client_step(&s,6000,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(!s.ap);
    assert(wifi_client_step(&s,10000,false,false,0,false)==(WIFI_CLIENT_DISCONNECT|WIFI_CLIENT_AP_ON));
    wifi_client_begin(&s,2,0,90,60);
    assert(!s.ap && s.phase==WIFI_PHASE_OFF);
    assert(!wifi_client_step(&s,UINT32_MAX,false,true,0,false));
    wifi_client_begin(&s,1,UINT32_MAX-999,2,10);
    assert(wifi_client_next_seconds(&s,UINT32_MAX-999)==2);
    assert(!wifi_client_step(&s,999,false,false,0,false));
    assert(wifi_client_step(&s,1000,false,false,0,false)==WIFI_CLIENT_CONNECT);
    assert(!s.ap);
    assert(wifi_client_cancel_for_upload(WIFI_CLIENT_ROUTER,true,false));
    assert(!wifi_client_cancel_for_upload(WIFI_CLIENT_ROUTER,true,true));
    assert(!wifi_client_cancel_for_upload(WIFI_CLIENT_ROUTER,false,false));
    assert(!wifi_client_cancel_for_upload(WIFI_CLIENT_AP,true,false));
    assert(!wifi_client_cancel_for_upload(WIFI_CLIENT_OFF,false,false));
    assert(!wifi_client_step(&s,12000,false,true,0,true));
    assert(s.phase==WIFI_PHASE_CONNECTING && !s.ap);
    assert(wifi_client_step(&s,12000,false,true,0,false)==(WIFI_CLIENT_DISCONNECT|WIFI_CLIENT_AP_ON));
    assert(s.ap);
    assert(!wifi_client_step(&s,13000,true,false,2,true)); /* No AP retirement during OTA. */
    assert(s.ap);
    assert(wifi_client_step(&s,13001,true,false,2,false)==WIFI_CLIENT_AP_OFF);
    wifi_client_begin(&s,2,0,0,60);
    assert(!wifi_client_step(&s,600000,false,true,2,false));
    puts("WiFi client policy: STA-only delay, DHCP timeout/success/recovery, AP fallback, retries, off, OTA and wrap passed");
    return 0;
}
