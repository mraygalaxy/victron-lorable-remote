#include "lora_power.h"
extern "C" {
#include "LoRaMac.h"
#include "region/RegionCommon.h"
}

namespace {
#if defined(LORABLE_MANUAL_RADIO_TEST)
bool manualRadioActive;
#endif
// High bit marks a newly configured ceiling; low five bits hold 0..22 dBm.
uint8_t requestedPower = 14 | 0x80;
int8_t configuredPower = INT8_MIN;
LoRaMacNvmData_t *powerContexts() {
    MibRequestConfirm_t m = {};
    m.Type = MIB_NVM_CTXS;
    if (LoRaMacMibGetRequestConfirm(&m) != LORAMAC_STATUS_OK) return nullptr;
    return m.Param.Contexts;
}
}

#if defined(LORABLE_MANUAL_RADIO_TEST)
bool loraManualRadioBegin() {
    if (manualRadioActive || LoRaMacIsBusy()) return false;
    LoRaMacNvmData_t *n = powerContexts();
    if (!n) return false;
    manualRadioActive = true;
    return true;
}

void loraManualRadioEnd() { manualRadioActive = false; }
#endif

// RUI's duty-cycle test switch alone still limits OTAA joins, and aggregate
// backoff is checked independently. Link wrapping this common selector avoids
// both waits only for the explicitly bracketed manual operation. The installed
// BSP is unchanged. Automatic requests call the original implementation.
extern "C" LoRaMacStatus_t __real_RegionCommonIdentifyChannels(
    RegionCommonIdentifyChannelsParam_t*, TimerTime_t*, uint8_t*, uint8_t*,
    uint8_t*, TimerTime_t*);
extern "C" LoRaMacStatus_t __wrap_RegionCommonIdentifyChannels(
    RegionCommonIdentifyChannelsParam_t *p, TimerTime_t *aggregatedTimeOff,
    uint8_t *enabledChannels, uint8_t *nbEnabledChannels,
    uint8_t *nbRestrictedChannels, TimerTime_t *nextTxDelay) {
#if defined(LORABLE_MANUAL_RADIO_TEST)
    if (manualRadioActive && p->MaxBands <= 8) {
        // Preserve band credits, readiness, aggregate debt and duty setting.
        // Only readiness is bypassed; masks, frequency and DR checks stay intact.
        RegionCommonCountNbOfEnabledChannelsParams_t *count = p->CountNbOfEnabledChannelsParam;
        uint8_t readiness = 0;
        for (uint8_t i = 0; i < p->MaxBands; ++i) {
            if (count->Bands[i].ReadyForTransmission) readiness |= (uint8_t)(1U << i);
            count->Bands[i].ReadyForTransmission = true;
        }
        RegionCommonCountNbOfEnabledChannels(count, enabledChannels,
            nbEnabledChannels, nbRestrictedChannels);
        for (uint8_t i = 0; i < p->MaxBands; ++i)
            count->Bands[i].ReadyForTransmission = (readiness & (1U << i)) != 0;
        *nextTxDelay = 0;
        return *nbEnabledChannels ? LORAMAC_STATUS_OK : LORAMAC_STATUS_NO_CHANNEL_FOUND;
    }
#endif
    return __real_RegionCommonIdentifyChannels(p, aggregatedTimeOff,
        enabledChannels, nbEnabledChannels, nbRestrictedChannels, nextTxDelay);
}

void loraConfigurePower(uint8_t dbm) {
    requestedPower = (dbm > 22 ? 22 : dbm) | 0x80;
    configuredPower = INT8_MIN;
}

bool loraApplyPower() {
    if (LoRaMacIsBusy()) return false;
    LoRaMacNvmData_t *n = powerContexts();
    if (!n) return false;
    const uint8_t ceiling = requestedPower & 0x1F;
    MibRequestConfirm_t m = {};
    m.Type = MIB_CHANNELS_DEFAULT_TX_POWER;
    m.Param.ChannelsDefaultTxPower = 0;
    if (LoRaMacMibSetRequestConfirm(&m) != LORAMAC_STATUS_OK) return false;
    // Preserve the live power index chosen by ADR; resetting it on each send
    // would silently defeat the user's ADR setting. Without ADR use the ceiling.
    if (!n->MacGroup2.AdrCtrlOn) {
        m.Type = MIB_CHANNELS_TX_POWER;
        m.Param.ChannelsTxPower = 0;
        if (LoRaMacMibSetRequestConfirm(&m) != LORAMAC_STATUS_OK) return false;
    }
    m.Type = MIB_DEFAULT_ANTENNA_GAIN;
    m.Param.DefaultAntennaGain = 0.0f;
    if (LoRaMacMibSetRequestConfirm(&m) != LORAMAC_STATUS_OK) return false;
    m.Type = MIB_ANTENNA_GAIN;
    m.Param.AntennaGain = 0.0f;
    if (LoRaMacMibSetRequestConfirm(&m) != LORAMAC_STATUS_OK) return false;
    // RUI 4.2.4 / LoRaMac-node 4.7.0 has no MIB_MAX_EIRP. Its public
    // MIB_NVM_CTXS exposes these RAM fields. Every OTAA join restores current
    // values from defaults, so both must be changed before requesting a join.
    // Gain zero makes the setting conducted radio output; actual antenna gain
    // still contributes to radiated EIRP and must be accounted for by the user.
    // Regional code may further restrict this ceiling (e.g. KR920). US915 uses
    // a fixed regional ERP instead, so the final computation is also capped.
    n->MacGroup2.MacParamsDefaults.MaxEirp = ceiling;
    // Do not undo a lower TxParamSetupReq limit on later uplinks. Explicit
    // reconfiguration and a fresh OTAA join can establish the ceiling anew.
    if ((requestedPower & 0x80) || n->MacGroup2.MacParams.MaxEirp > ceiling)
        n->MacGroup2.MacParams.MaxEirp = ceiling;
    requestedPower = ceiling;
    // No RUI persistent setter: for OTAA, RUI's OnNvmDataChange does not save
    // MacGroup1/2. A later standard firmware boot regains regional defaults.
    return true;
}

extern "C" int8_t __real_RegionCommonComputeTxPower(int8_t, float, float);
extern "C" int8_t __wrap_RegionCommonComputeTxPower(int8_t index, float maxEirp, float gain) {
    int8_t dbm = __real_RegionCommonComputeTxPower(index, maxEirp, gain);
    const uint8_t ceiling = requestedPower & 0x1F;
    if (dbm > ceiling) dbm = ceiling;
    // RAK11160/11162's HP driver supports -9..22 dBm. Zero is a valid request.
    if (dbm < -9) dbm = -9;
    configuredPower = dbm;
    return dbm;
}

int8_t loraConfiguredPowerDbm() { return configuredPower; }
