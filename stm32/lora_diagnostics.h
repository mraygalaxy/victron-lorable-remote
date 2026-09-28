#pragma once
#include <stdint.h>

// Volatile observations only: no packet payloads, keys, or flash writes.
// A successful request is not a radio completion or a network acknowledgement.
struct LoraDiagnostics {
    uint32_t sequence = 0, txAt = 0, rxAt = 0, joinAt = 0, ackCount = 0;
    int32_t txCode = 0, joinCode = 0;
    int16_t rssi = 0;
    int8_t snr = 0;
    uint8_t txState = 0, ack = 0, txSlot = 255, rxSlot = 255, joinSlot = 255;
    bool rxSeen = false, joinSeen = false;

    void submit(uint32_t now, uint8_t slot, bool confirmed) {
        ++sequence; txAt = now; txSlot = slot; txState = 1; txCode = 0;
        ack = confirmed ? 1 : 0;
    }
    void rejected() { txState = 3; txCode = -1; ack = 0; }
    void complete(int32_t code, bool acknowledged) {
        if (txState != 1) return; // Unrelated/stale MAC completions are not this packet.
        txCode = code; txState = code == 0 ? 2 : 4;
        if (ack == 1) { ack = acknowledged ? 2 : 3; if (acknowledged) ++ackCount; }
    }
    void received(uint32_t now, uint8_t slot, int16_t strength, int8_t noiseRatio) {
        rxAt = now; rxSlot = slot; rssi = strength; snr = noiseRatio; rxSeen = true;
    }
    void joined(uint32_t now, uint8_t slot, int32_t code) {
        joinAt = now; joinSlot = slot; joinCode = code; joinSeen = true;
    }
};
static_assert(sizeof(LoraDiagnostics) <= 40, "Keep LoRa diagnostics within the RAM budget");
extern LoraDiagnostics loraDiagnostics;
