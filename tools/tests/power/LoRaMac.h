#pragma once
#include <stdint.h>
// Host-only subset for the fixed-power helper; never linked into firmware.
enum LoRaMacStatus_t { LORAMAC_STATUS_OK, LORAMAC_STATUS_ERROR,
    LORAMAC_STATUS_DUTYCYCLE_RESTRICTED, LORAMAC_STATUS_NO_CHANNEL_FOUND };
enum { LORAMAC_REGION_AS923, LORAMAC_REGION_AU915, LORAMAC_REGION_EU868,
    LORAMAC_REGION_KR920, LORAMAC_REGION_IN865, LORAMAC_REGION_US915,
    LORAMAC_REGION_RU864 };
enum { MIB_NVM_CTXS, MIB_ADR, MIB_CHANNELS_DEFAULT_TX_POWER,
       MIB_CHANNELS_TX_POWER, MIB_DEFAULT_ANTENNA_GAIN, MIB_ANTENNA_GAIN };
struct PowerMacParams { float MaxEirp, AntennaGain; };
struct LoRaMacNvmData_t {
    struct { int8_t ChannelsTxPower; } MacGroup1;
    struct {
        int Region;
        bool AdrCtrlOn;
        int8_t ChannelsTxPowerDefault;
        PowerMacParams MacParams, MacParamsDefaults;
    } MacGroup2;
};
struct MibRequestConfirm_t {
    int Type;
    union {
        LoRaMacNvmData_t *Contexts;
        bool AdrEnable;
        int8_t ChannelsDefaultTxPower, ChannelsTxPower;
        float AntennaGain, DefaultAntennaGain;
    } Param;
};
LoRaMacStatus_t LoRaMacMibGetRequestConfirm(MibRequestConfirm_t *m);
LoRaMacStatus_t LoRaMacMibSetRequestConfirm(MibRequestConfirm_t *m);
bool LoRaMacIsBusy();
