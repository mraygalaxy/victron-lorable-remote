#pragma once
#include <stdint.h>

// Conducted-output ceiling, not radiated EIRP. ADR/regional rules may reduce it.
void loraConfigurePower(uint8_t dbm);
bool loraApplyPower();
// Last radio-output request, not measured RF. INT8_MIN until configured for TX.
int8_t loraConfiguredPowerDbm();

#if defined(LORABLE_MANUAL_RADIO_TEST)
// Bracket one explicitly requested transmission, including its RX/retry cycle.
// Begin fails while the MAC is busy; End is required on rejection/completion.
// Only the manual-radio test build bypasses waits. No setting is persisted.
bool loraManualRadioBegin();
void loraManualRadioEnd();
#else
inline bool loraManualRadioBegin() { return true; }
inline void loraManualRadioEnd() {}
#endif
