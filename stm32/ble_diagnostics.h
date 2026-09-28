#pragma once

/* RAM-only diagnostic phase; shared by both firmware components.
 * Keep these numbers stable for the portal and older result readers.
 */
enum BleDiagnosticStage {
    BLE_STAGE_NONE = 0,
    BLE_STAGE_STACK = 1,
    BLE_STAGE_SCAN = 2,
    BLE_STAGE_CONNECT = 3,
    BLE_STAGE_SECURITY = 4,
    BLE_STAGE_MTU = 5,
    BLE_STAGE_SERVICES = 6,
    BLE_STAGE_CHARACTERISTICS = 7,
    BLE_STAGE_DESCRIPTORS = 8,
    BLE_STAGE_NOTIFICATIONS = 9,
    BLE_STAGE_CONTROL_INFO = 10,
    BLE_STAGE_TRANSPORT_INIT = 11,
    BLE_STAGE_TX_CREDIT = 12,
    BLE_STAGE_DEVICES = 13,
    BLE_STAGE_SUBSCRIBE = 14,
    BLE_STAGE_KEEPALIVE = 15,
    BLE_STAGE_INITIAL_READ = 16,
    BLE_STAGE_WRITE = 17,
    BLE_STAGE_READBACK = 18,
    BLE_STAGE_OUTPUT = 19,
    BLE_STAGE_SHUTDOWN = 20
};
