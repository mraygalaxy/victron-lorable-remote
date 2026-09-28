// Build with -Itools/tests/power; TEST_STANDARD disables only manual duty bypass.
#ifndef TEST_STANDARD
#define LORABLE_MANUAL_RADIO_TEST 1
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../stm32/lora_power.cpp"

static LoRaMacNvmData_t radioContext;
static bool busy, failGet, nullContext;
static int failSet = -1;
static unsigned writes;
static unsigned normalChannelCalls;
extern "C" {
#ifdef TEST_VENDOR_REAL
#include "radio.h"
// The linked vendor file also contains unused beacon helpers. No RF is used.
const struct Radio_s Radio = {};
#endif
int8_t __real_RegionCommonComputeTxPower(int8_t index, float maxEirp, float gain) {
#ifdef TEST_VENDOR_REAL
    return RegionCommonComputeTxPower(index, maxEirp, gain);
#else
    return (int8_t)floor((maxEirp - 2 * index) - gain);
#endif
}
LoRaMacStatus_t __real_RegionCommonIdentifyChannels(
    RegionCommonIdentifyChannelsParam_t *p, TimerTime_t *aggregate,
    uint8_t *channels, uint8_t *enabled, uint8_t *restricted, TimerTime_t *delay) {
    ++normalChannelCalls;
#ifdef TEST_VENDOR_REAL
    return RegionCommonIdentifyChannels(p, aggregate, channels, enabled, restricted, delay);
#else
    (void)channels;
    *aggregate = p->AggrTimeOff; *enabled = 0; *restricted = 1; *delay = 60000;
    return LORAMAC_STATUS_DUTYCYCLE_RESTRICTED;
#endif
}
#ifdef TEST_VENDOR_REAL
TimerTime_t TimerGetCurrentTime() { return 100001; }
TimerTime_t TimerGetElapsedTime(TimerTime_t past) { return TimerGetCurrentTime() - past; }
#else
void RegionCommonCountNbOfEnabledChannels(
    RegionCommonCountNbOfEnabledChannelsParams_t *p, uint8_t *channels,
    uint8_t *enabled, uint8_t *restricted) {
    // Mirror the vendor channel filter, including all its selection predicates.
    *enabled = *restricted = 0;
    for (uint16_t i = 0; i < p->MaxNbChannels; ++i) {
        const ChannelParams_t &channel = p->Channels[i];
        if (!(p->ChannelsMask[i / 16] & (1U << (i % 16))) || !channel.Frequency) continue;
        if (!p->Joined && p->JoinChannels && !(p->JoinChannels[i / 16] & (1U << (i % 16)))) continue;
        if (p->Datarate < channel.DrRange.Fields.Min || p->Datarate > channel.DrRange.Fields.Max) continue;
        if (!p->Bands[channel.Band].ReadyForTransmission) { ++*restricted; continue; }
        channels[(*enabled)++] = (uint8_t)i;
    }
}
#endif
bool LoRaMacIsBusy() { return busy; }
LoRaMacStatus_t LoRaMacMibGetRequestConfirm(MibRequestConfirm_t *m) {
    assert(m->Type == MIB_NVM_CTXS);
    if (failGet) return LORAMAC_STATUS_ERROR;
    m->Param.Contexts = nullContext ? nullptr : &radioContext;
    return LORAMAC_STATUS_OK;
}
LoRaMacStatus_t LoRaMacMibSetRequestConfirm(MibRequestConfirm_t *m) {
    if (m->Type == failSet) return LORAMAC_STATUS_ERROR;
    ++writes;
    switch (m->Type) {
    case MIB_ADR: radioContext.MacGroup2.AdrCtrlOn = m->Param.AdrEnable; break;
    case MIB_CHANNELS_DEFAULT_TX_POWER: radioContext.MacGroup2.ChannelsTxPowerDefault = m->Param.ChannelsDefaultTxPower; break;
    case MIB_CHANNELS_TX_POWER: radioContext.MacGroup1.ChannelsTxPower = m->Param.ChannelsTxPower; break;
    case MIB_DEFAULT_ANTENNA_GAIN: radioContext.MacGroup2.MacParamsDefaults.AntennaGain = m->Param.DefaultAntennaGain; break;
    case MIB_ANTENNA_GAIN: radioContext.MacGroup2.MacParams.AntennaGain = m->Param.AntennaGain; break;
    default: assert(false);
    }
    return LORAMAC_STATUS_OK;
}
}
static void staleSettings() {
    radioContext = {};
    radioContext.MacGroup2.Region = LORAMAC_REGION_EU868;
    radioContext.MacGroup2.AdrCtrlOn = true;
    radioContext.MacGroup1.ChannelsTxPower = 3;
    radioContext.MacGroup2.ChannelsTxPowerDefault = 5;
    radioContext.MacGroup2.MacParams.MaxEirp = 16.0f;
    radioContext.MacGroup2.MacParams.AntennaGain = 2.15f;
    radioContext.MacGroup2.MacParamsDefaults.MaxEirp = 16.0f;
    radioContext.MacGroup2.MacParamsDefaults.AntennaGain = 2.15f;
    busy = failGet = nullContext = false;
    failSet = -1; writes = 0;
    loraConfigurePower(14);
}
static int8_t configuredRequest() {
    return __wrap_RegionCommonComputeTxPower(radioContext.MacGroup1.ChannelsTxPower,
        radioContext.MacGroup2.MacParams.MaxEirp, radioContext.MacGroup2.MacParams.AntennaGain);
}
static void assertCeiling(uint8_t ceiling) {
    assert(radioContext.MacGroup2.ChannelsTxPowerDefault == 0);
    assert(radioContext.MacGroup2.MacParams.MaxEirp == ceiling);
    assert(radioContext.MacGroup2.MacParamsDefaults.MaxEirp == ceiling);
    assert(radioContext.MacGroup2.MacParams.AntennaGain == 0.0f);
    assert(radioContext.MacGroup2.MacParamsDefaults.AntennaGain == 0.0f);
}
static void powerTests() {
    for (uint8_t ceiling = 0; ceiling <= 22; ++ceiling) {
        staleSettings(); loraConfigurePower(ceiling);
        assert(loraConfiguredPowerDbm() == INT8_MIN);
        assert(loraApplyPower()); assertCeiling(ceiling);
        assert(radioContext.MacGroup2.AdrCtrlOn && radioContext.MacGroup1.ChannelsTxPower == 3);
        assert(loraConfiguredPowerDbm() == INT8_MIN); // Applying settings is not TX.
        assert(configuredRequest() == (int)ceiling - 6);
        assert(loraConfiguredPowerDbm() == (int)ceiling - 6);
        assert(loraApplyPower() && radioContext.MacGroup1.ChannelsTxPower == 3);
        // OTAA reset restores configured defaults, including index zero.
        radioContext.MacGroup1.ChannelsTxPower = radioContext.MacGroup2.ChannelsTxPowerDefault;
        radioContext.MacGroup2.MacParams = radioContext.MacGroup2.MacParamsDefaults;
        assertCeiling(ceiling); assert(configuredRequest() == ceiling);
        // With ADR disabled, stale reduced indices are cleared on each apply.
        radioContext.MacGroup2.AdrCtrlOn = false;
        radioContext.MacGroup1.ChannelsTxPower = 7;
        assert(loraApplyPower()); assertCeiling(ceiling);
        assert(!radioContext.MacGroup2.AdrCtrlOn && radioContext.MacGroup1.ChannelsTxPower == 0);
        assert(configuredRequest() == ceiling && loraConfiguredPowerDbm() == ceiling);
        // US915 uses a hard-coded regional ERP. Final cap must still hold.
        assert(__wrap_RegionCommonComputeTxPower(0, 30.0f, 0.0f) == ceiling);
    }
    staleSettings(); loraConfigurePower(255); assert(loraApplyPower()); assertCeiling(22);
    // Regional reductions remain authoritative (KR920 uses min with its band limit).
    assert(__wrap_RegionCommonComputeTxPower(0, 14.0f, 0.0f) == 14);
    assert(loraConfiguredPowerDbm() == 14);
    radioContext.MacGroup2.MacParams.MaxEirp = 8; // Server TxParamSetupReq.
    assert(loraApplyPower() && radioContext.MacGroup2.MacParams.MaxEirp == 8);
    assert(radioContext.MacGroup2.MacParamsDefaults.MaxEirp == 22);
    loraConfigurePower(14); assert(loraApplyPower()); assertCeiling(14);
    loraConfigurePower(0); assert(loraConfiguredPowerDbm() == INT8_MIN);
    assert(__wrap_RegionCommonComputeTxPower(7, 0.0f, 0.0f) == -9); // HP hardware minimum.
    assert(loraConfiguredPowerDbm() == -9);
    const decltype(radioContext.MacGroup2.Region) regions[] = {
        LORAMAC_REGION_EU868, LORAMAC_REGION_US915, LORAMAC_REGION_AU915,
        LORAMAC_REGION_KR920, LORAMAC_REGION_AS923, LORAMAC_REGION_IN865, LORAMAC_REGION_RU864};
    for (auto region : regions) {
        staleSettings(); radioContext.MacGroup2.Region = region;
        assert(loraApplyPower()); assertCeiling(14);
    }
    staleSettings(); busy = true; assert(!loraApplyPower() && writes == 0);
    staleSettings(); failGet = true; assert(!loraApplyPower() && writes == 0);
    staleSettings(); nullContext = true; assert(!loraApplyPower() && writes == 0);
    const int setters[] = {MIB_CHANNELS_DEFAULT_TX_POWER, MIB_CHANNELS_TX_POWER,
        MIB_DEFAULT_ANTENNA_GAIN, MIB_ANTENNA_GAIN};
    for (int type : setters) {
        staleSettings(); radioContext.MacGroup2.AdrCtrlOn = false;
        failSet = type; assert(!loraApplyPower());
        assert(radioContext.MacGroup2.MacParams.MaxEirp == 16.0f);
    }
}
static void manualRadioTests() {
    staleSettings();
#ifdef TEST_STANDARD
    RegionCommonIdentifyChannelsParam_t p = {};
    uint8_t channels[16] = {}, enabled = 0, restricted = 0;
    TimerTime_t aggregate = 0, delay = 0;
    assert(loraManualRadioBegin());
    assert(__wrap_RegionCommonIdentifyChannels(&p, &aggregate, channels,
        &enabled, &restricted, &delay) == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED);
    loraManualRadioEnd();
#else
    Band_t bands[2] = {};
    bands[1].TimeCredits = 21;
    bands[1].ReadyForTransmission = true;
    ChannelParams_t channels[16] = {};
    for (unsigned i = 0; i < 6; ++i) {
        channels[i].Frequency = 868100000 + i * 200000;
        channels[i].DrRange.Fields.Max = 5;
        channels[i].Band = i & 1;
    }
    channels[3].Frequency = 0; // No defined frequency.
    channels[4].DrRange.Fields.Min = 1; // Wrong datarate.
    uint16_t mask = 0x1F, joinMask = 0x19; // #5 masked; #1/#2 data-only.
    RegionCommonCountNbOfEnabledChannelsParams_t count = {false, 0, &mask,
        channels, bands, 16, &joinMask};
    RegionCommonIdentifyChannelsParam_t p = {};
    p.AggrTimeOff = 120000; p.LastAggrTx = 100000;
    p.DutyCycleEnabled = true; p.MaxBands = 2;
    p.CountNbOfEnabledChannelsParam = &count;
    uint8_t enabledChannels[16] = {}, enabled = 0, restricted = 0;
    TimerTime_t aggregate = p.AggrTimeOff, delay = 0;
    const Band_t savedBands[2] = {bands[0], bands[1]};
    const LoRaMacNvmData_t savedContext = radioContext;
    auto select = [&]() { return __wrap_RegionCommonIdentifyChannels(&p,
        &aggregate, enabledChannels, &enabled, &restricted, &delay); };
    assert(select() == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED);
    assert(normalChannelCalls == 1 && delay > 0);
    busy = true; assert(!loraManualRadioBegin()); busy = false;
    assert(loraManualRadioBegin()); assert(!loraManualRadioBegin());
    assert(select() == LORAMAC_STATUS_OK && enabled == 1 && enabledChannels[0] == 0);
    assert(delay == 0 && aggregate == p.AggrTimeOff && normalChannelCalls == 1);
    assert(!memcmp(savedBands, bands, sizeof(bands)));
    assert(!memcmp(&savedContext, &radioContext, sizeof(radioContext)) && writes == 0);
    count.Joined = true;
    assert(select() == LORAMAC_STATUS_OK && enabled == 3);
    assert(enabledChannels[0] == 0 && enabledChannels[1] == 1 && enabledChannels[2] == 2);
    mask = 0;
    assert(select() == LORAMAC_STATUS_NO_CHANNEL_FOUND && enabled == 0);
    assert(!memcmp(savedBands, bands, sizeof(bands)));
    loraManualRadioEnd(); loraManualRadioEnd();
    assert(select() == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED && normalChannelCalls == 2);
#ifdef TEST_VENDOR_REAL
    // Vendor duty=false alone still rejects an unjoined node with exhausted
    // band credits, even without aggregate debt. The manual scope bypasses
    // this separate join limit, then the original restriction returns.
    mask = 1; count.Joined = false; p.AggrTimeOff = 0;
    p.DutyCycleEnabled = false; p.ExpectedTimeOnAir = 1500;
    p.ElapsedTimeSinceStartUp.Seconds = 10;
    for (Band_t &band : bands) {
        band.DCycle = 100; band.TimeCredits = 0; band.MaxTimeCredits = 3600000;
        band.LastBandUpdateTime = 100000; band.LastMaxCreditAssignTime = 3600000;
    }
    assert(select() == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED);
    const Band_t exhaustedBands[2] = {bands[0], bands[1]};
    assert(loraManualRadioBegin());
    assert(select() == LORAMAC_STATUS_OK && enabled == 1 && delay == 0);
    assert(!memcmp(exhaustedBands, bands, sizeof(bands)));
    loraManualRadioEnd();
    assert(select() == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED);
#endif
    failGet = true; assert(!loraManualRadioBegin()); failGet = false;
    nullContext = true; assert(!loraManualRadioBegin()); nullContext = false;
    radioContext.MacGroup2.Region = LORAMAC_REGION_KR920;
    assert(loraManualRadioBegin()); loraManualRadioEnd();
#endif
}

int main() {
    powerTests();
    manualRadioTests();
#ifdef TEST_VENDOR_REAL
    puts("PASS: actual installed RUI 4.2.4 RegionCommon.c channel-selection implementation");
#endif
    puts("PASS: 0..22 dBm ceilings, real zero, ADR preserved/lower, OTAA defaults, US915 final cap, regional/server reductions, hardware bounds/status");
#ifdef TEST_STANDARD
    puts("PASS: power control retained with manual duty-cycle bypass disabled");
#else
    puts("PASS: manual-only duty/aggregate bypass, normal fallback, busy queue, masks/DR/frequency validation, no band/context writes");
#endif
}
