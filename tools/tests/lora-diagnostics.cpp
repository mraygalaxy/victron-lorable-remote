#include <assert.h>
#include <stdio.h>
#include "../../stm32/lora_diagnostics.h"

int main() {
    LoraDiagnostics d;
    assert(!d.rxSeen && !d.joinSeen && d.txState == 0 && d.txSlot == 255);
    d.complete(0, true); assert(d.ackCount == 0 && d.txState == 0);
    d.submit(10, 1, true); assert(d.sequence == 1 && d.txState == 1 && d.ack == 1);
    d.complete(0, false); assert(d.txState == 2 && d.ack == 3 && d.ackCount == 0);
    d.complete(0, true); assert(d.ack == 3 && d.ackCount == 0);
    d.submit(20, 0, true); d.complete(0, true);
    assert(d.txSlot == 0 && d.ack == 2 && d.ackCount == 1);
    d.submit(30, 0, false); d.complete(0, true);
    assert(d.ack == 0 && d.ackCount == 1); // Cached ACK cannot confirm an unconfirmed frame.
    d.submit(40, 0, true); d.rejected();
    assert(d.txState == 3 && d.ack == 0 && d.txCode == -1);
    d.complete(0, true); assert(d.txState == 3 && d.ackCount == 1);
    d.submit(50, 1, true); d.complete(3, false);
    assert(d.txState == 4 && d.txCode == 3 && d.ack == 3);
    d.received(51, 1, -119, -14);
    assert(d.rxSeen && d.rssi == -119 && d.snr == -14 && d.rxAt == 51 && d.rxSlot == 1);
    d.joined(55, 0, 6);
    assert(d.joinSeen && d.joinCode == 6 && d.joinSlot == 0 && d.rxAt == 51);
    d.joined(65, 0, 0); assert(d.joinCode == 0 && d.rxSlot == 1); // Join does not invent RSSI.
    d.txAt = 0xfffffff0u; assert((uint32_t)(0x10u - d.txAt) == 32);
    puts("LoRa diagnostics: accepted / completion / ACK / RX / join scenarios passed");
}
