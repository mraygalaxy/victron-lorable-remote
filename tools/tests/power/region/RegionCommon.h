#pragma once
#include "LoRaMac.h"
typedef uint32_t TimerTime_t;
struct Band_t {
    uint32_t TimeCredits;
    bool ReadyForTransmission;
};
struct ChannelParams_t {
    uint32_t Frequency;
    struct { struct { uint8_t Min, Max; } Fields; } DrRange;
    uint8_t Band;
};
struct RegionCommonCountNbOfEnabledChannelsParams_t {
    bool Joined;
    uint8_t Datarate;
    uint16_t *ChannelsMask;
    ChannelParams_t *Channels;
    Band_t *Bands;
    uint16_t MaxNbChannels;
    uint16_t *JoinChannels;
};
struct RegionCommonIdentifyChannelsParam_t {
    TimerTime_t AggrTimeOff, LastAggrTx;
    bool DutyCycleEnabled;
    uint8_t MaxBands;
    bool LastTxIsJoinRequest;
    TimerTime_t ExpectedTimeOnAir;
    RegionCommonCountNbOfEnabledChannelsParams_t *CountNbOfEnabledChannelsParam;
};
void RegionCommonCountNbOfEnabledChannels(
    RegionCommonCountNbOfEnabledChannelsParams_t*, uint8_t*, uint8_t*, uint8_t*);
