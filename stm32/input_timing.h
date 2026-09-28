#pragma once
#include <stdint.h>

// One cancellable action per stable edge. The opposite edge replaces it;
// holding a level never re-arms it. All times and state are RAM-only.
struct InputActionTimer {
    uint32_t due=0;
    bool pending=false, rising=false;
    void edge(uint32_t at, bool level, uint16_t holdSeconds, uint16_t delaySeconds) {
        rising=level;pending=true;
        due=at+((uint32_t)holdSeconds+delaySeconds)*1000UL;
    }
    bool take(uint32_t now,bool level) {
        if(!pending)return false;
        if(level!=rising){pending=false;return false;}
        if((int32_t)(now-due)<0)return false;
        pending=false;return true;
    }
};
