#pragma once
#include "config_store.h"
void networkBegin(const RuntimeConfig &config,const uint8_t devEui[8]);
// Queue a RAM-only rejoin. 255 selects the highest-priority valid profile.
// Always waits for an active radio operation. The fixed-power test variant
// permits one manual attempt outside the automatic scheduler's join limits.
bool networkJoinNow(uint8_t slot=255);
bool networkManualJoinPending();
// Latest LoRaMAC request result, -1 if none; ERROR also covers preparation failure.
int32_t networkJoinRequestCode();
void networkTick(bool transmissionInFlight);
void networkJoinResult(int32_t result);
void networkTxComplete(bool healthProbe,bool acknowledged);
void networkDownlinkReceived();
bool networkJoined();
bool networkHealthDue();
uint8_t networkActiveSlot();
uint8_t networkState();
uint8_t networkMissedChecks();
uint32_t networkPreemptRemaining();
uint32_t networkRetryRemaining();
uint32_t networkStatusIntervalMinutes();
