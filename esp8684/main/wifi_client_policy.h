#pragma once
#include <stdbool.h>
#include <stdint.h>

/* RAM-only policy; milliseconds wrap safely. Radio work stays in portal.c. */
enum { WIFI_CLIENT_AP = 0, WIFI_CLIENT_ROUTER = 1, WIFI_CLIENT_OFF = 2 };
enum { WIFI_PHASE_OFF, WIFI_PHASE_AP, WIFI_PHASE_DELAY, WIFI_PHASE_CONNECTING,
       WIFI_PHASE_CONNECTED, WIFI_PHASE_FALLBACK };
enum { WIFI_CLIENT_CONNECT = 1, WIFI_CLIENT_DISCONNECT = 2,
       WIFI_CLIENT_AP_ON = 4, WIFI_CLIENT_AP_OFF = 8 };
typedef struct {
    uint32_t due, window_end, retry_at, timeout_ms;
    uint8_t mode, phase;
    bool attempting, ap, fallback;
} wifi_client_policy_t;

static inline bool wifi_client_due(uint32_t now, uint32_t deadline)
{ return (int32_t)(now - deadline) >= 0; }

static inline bool wifi_client_cancel_for_upload(uint8_t mode, bool started, bool connected)
{ return mode==WIFI_CLIENT_ROUTER && started && !connected; }

static inline void wifi_client_begin(wifi_client_policy_t *s, uint8_t mode,
                                     uint32_t now, uint16_t delay_s, uint16_t timeout_s)
{
    *s = (wifi_client_policy_t){.mode=mode, .timeout_ms=(uint32_t)timeout_s*1000,
        .due=now+(uint32_t)delay_s*1000, .ap=mode==WIFI_CLIENT_AP,
        .phase=mode==WIFI_CLIENT_OFF ? WIFI_PHASE_OFF :
               mode==WIFI_CLIENT_AP ? WIFI_PHASE_AP : WIFI_PHASE_DELAY};
}

static inline unsigned wifi_client_step(wifi_client_policy_t *s, uint32_t now,
                                        bool connected, bool disconnected,
                                        unsigned ap_clients, bool freeze)
{
    (void)ap_clients; /* A fallback AP always retires after successful DHCP. */
    if (s->mode != WIFI_CLIENT_ROUTER || freeze) return 0;
    unsigned action = 0;
    if (connected) {
        s->phase = WIFI_PHASE_CONNECTED;
        s->fallback=false;
        s->attempting = false;
        if (s->ap) {s->ap=false;action|=WIFI_CLIENT_AP_OFF;}
        return action;
    }
    if (s->phase == WIFI_PHASE_CONNECTED) {
        s->phase = WIFI_PHASE_FALLBACK;
        s->fallback=true;
        s->due = now + 5000;
        s->attempting = false;
        if(!s->ap){s->ap=true;action|=WIFI_CLIENT_AP_ON;}
    }
    if(s->fallback && !s->ap){s->ap=true;action|=WIFI_CLIENT_AP_ON;}
    if (s->phase == WIFI_PHASE_CONNECTING) {
        if (wifi_client_due(now,s->window_end)) {
            s->phase=WIFI_PHASE_FALLBACK;s->due=now+60000;
            s->fallback=true;
            if(s->attempting)action|=WIFI_CLIENT_DISCONNECT;
            s->attempting=false;
            if(!s->ap){s->ap=true;action|=WIFI_CLIENT_AP_ON;}
            return action;
        }
        if(disconnected) {s->attempting=false;s->retry_at=now+5000;}
        if(!s->attempting && wifi_client_due(now,s->retry_at)) {
            s->attempting=true;action|=WIFI_CLIENT_CONNECT;
        }
    } else if (wifi_client_due(now,s->due)) {
        s->phase=WIFI_PHASE_CONNECTING;s->window_end=now+s->timeout_ms;
        s->attempting=true;action|=WIFI_CLIENT_CONNECT;
    }
    return action;
}

static inline uint32_t wifi_client_next_seconds(const wifi_client_policy_t *s, uint32_t now)
{
    uint32_t due = (s->phase==WIFI_PHASE_DELAY || s->phase==WIFI_PHASE_FALLBACK) ?
        s->due : s->phase==WIFI_PHASE_CONNECTING ? s->window_end : now;
    return wifi_client_due(now,due) ? 0 : (due-now+999)/1000;
}
