#pragma once
/* Minimal deterministic SDK surface for victron-session.c only.
 * This is not a Bluetooth stack: the production C state machine is compiled
 * unchanged, while the test supplies ATT/GAP callbacks and an artificial clock.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NO_MEM 0x101
typedef uint32_t TickType_t;
typedef struct { bool available, deleted; } test_semaphore;
typedef test_semaphore *SemaphoreHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreGive(SemaphoreHandle_t);
int xSemaphoreTake(SemaphoreHandle_t, TickType_t);
void vSemaphoreDelete(SemaphoreHandle_t);
void vTaskDelay(TickType_t);
int64_t esp_timer_get_time(void);

typedef struct { uint8_t type; uint8_t val[6]; } ble_addr_t;
typedef struct { uint8_t type; char text[37]; } ble_uuid_t;
typedef struct { ble_uuid_t u; uint16_t value; } ble_uuid16_t;
typedef struct { ble_uuid_t u; uint32_t value; } ble_uuid32_t;
typedef union { ble_uuid_t u; ble_uuid16_t u16; ble_uuid32_t u32; } ble_uuid_any_t;
#define BLE_UUID_TYPE_16 16
#define BLE_UUID_TYPE_32 32
#define BLE_UUID_TYPE_128 128
#define BLE_UUID_STR_LEN 37
#define BLE_UUID16(p) ((const ble_uuid16_t *)(p))
#define BLE_UUID32(p) ((const ble_uuid32_t *)(p))
int ble_uuid_from_str(ble_uuid_any_t *,const char *);
char *ble_uuid_to_str(const ble_uuid_t *,char *);
uint16_t ble_uuid_u16(const ble_uuid_t *);

#define BLE_ADDR_PUBLIC 0
#define BLE_ADDR_RANDOM 1
#define BLE_HS_CONN_HANDLE_NONE 0xffff
#define BLE_HS_EUNKNOWN 17
#define BLE_HS_ECONTROLLER 12
#define BLE_HS_EBADDATA 10
#define BLE_HS_EDONE 14
#define BLE_HS_EALREADY 2
#define BLE_HS_EBUSY 15
#define BLE_HS_ETIMEOUT 13
#define BLE_HS_ENOTCONN 7
#define BLE_HS_EAUTHEN 23
#define BLE_HS_ENOTSUP 8
#define BLE_HS_ATT_ERR(x) (0x100+(x))
#define BLE_ATT_ERR_REQ_NOT_SUPPORTED 6
#define BLE_ATT_MTU_DFLT 23
#define BLE_ERR_REM_USER_CONN_TERM 0x13
#define BLE_HS_IO_KEYBOARD_ONLY 2
#define BLE_HS_IO_NO_INPUT_OUTPUT 3
#define BLE_SM_IOACT_INPUT 1
#define BLE_SM_IOACT_STATIC 2
#define BLE_SM_IOACT_DISP 3
#define BLE_SM_IOACT_NUMCMP 4
#define BLE_GATT_CHR_PROP_READ 2
#define BLE_GATT_CHR_PROP_WRITE_NO_RSP 4
#define BLE_GATT_CHR_PROP_WRITE 8
#define BLE_GATT_CHR_PROP_NOTIFY 16
#define BLE_GATT_DSC_CLT_CFG_UUID16 0x2902
#define BLE_GAP_EVENT_DISC 1
#define BLE_GAP_EVENT_DISC_COMPLETE 2
#define BLE_GAP_EVENT_CONNECT 3
#define BLE_GAP_EVENT_DISCONNECT 4
#define BLE_GAP_EVENT_ENC_CHANGE 5
#define BLE_GAP_EVENT_PASSKEY_ACTION 6
#define BLE_GAP_EVENT_NOTIFY_RX 7
#define BLE_GAP_EVENT_MTU 8

struct os_mbuf { const uint8_t *data; uint16_t len; };
#define OS_MBUF_PKTLEN(p) ((p)->len)
int os_mbuf_copydata(const struct os_mbuf *,int,int,void *);
struct ble_gap_disc_params { uint16_t itvl,window; uint8_t filter_policy,limited,passive,filter_duplicates; };
struct ble_gap_sec_state { unsigned encrypted:1,authenticated:1,bonded:1,key_size:5; };
struct ble_gap_conn_desc { struct ble_gap_sec_state sec_state; uint16_t conn_handle; };
struct ble_gap_event {
    int type;
    struct { ble_addr_t addr; int8_t rssi; uint8_t event_type; } disc;
    struct { int status; uint16_t conn_handle; } connect,enc_change;
    struct { int reason; struct ble_gap_conn_desc conn; } disconnect;
    struct { uint16_t conn_handle; struct { int action; } params; } passkey;
    struct { uint16_t conn_handle,attr_handle; struct os_mbuf *om; } notify_rx;
    struct { uint16_t conn_handle,value; } mtu;
};
typedef int ble_gap_event_fn(struct ble_gap_event *,void *);
struct ble_sm_io { int action; uint32_t passkey; int numcmp_accept; };
struct test_hs_cfg {
    void (*reset_cb)(int); void (*sync_cb)(void);
    uint8_t sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_sc_only;
};
extern struct test_hs_cfg ble_hs_cfg;
int ble_hs_util_ensure_addr(int);
int ble_hs_id_infer_auto(int,uint8_t *);
int ble_gap_disc(uint8_t,int32_t,const struct ble_gap_disc_params *,ble_gap_event_fn *,void *);
int ble_gap_disc_cancel(void);
int ble_gap_connect(uint8_t,const ble_addr_t *,int32_t,const void *,ble_gap_event_fn *,void *);
int ble_gap_conn_cancel(void);
int ble_gap_terminate(uint16_t,uint8_t);
int ble_gap_security_initiate(uint16_t);
int ble_gap_conn_find(uint16_t,struct ble_gap_conn_desc *);
int ble_sm_inject_io(uint16_t,struct ble_sm_io *);
int ble_sm_configure_static_passkey(uint32_t,bool);
int nimble_port_init(void);
int nimble_port_stop(void);
int nimble_port_deinit(void);
void nimble_port_run(void);
void nimble_port_freertos_init(void (*)(void *));
void nimble_port_freertos_deinit(void);

struct ble_gatt_error { int status; uint16_t att_handle; };
struct ble_gatt_svc { uint16_t start_handle,end_handle; ble_uuid_any_t uuid; };
struct ble_gatt_chr { uint16_t def_handle,val_handle; uint8_t properties; ble_uuid_any_t uuid; };
struct ble_gatt_dsc { uint16_t handle; ble_uuid_any_t uuid; };
struct ble_gatt_attr { uint16_t handle; struct os_mbuf *om; };
typedef int ble_gatt_disc_svc_fn(uint16_t,const struct ble_gatt_error *,const struct ble_gatt_svc *,void *);
typedef int ble_gatt_chr_fn(uint16_t,const struct ble_gatt_error *,const struct ble_gatt_chr *,void *);
typedef int ble_gatt_dsc_fn(uint16_t,const struct ble_gatt_error *,uint16_t,const struct ble_gatt_dsc *,void *);
typedef int ble_gatt_attr_fn(uint16_t,const struct ble_gatt_error *,struct ble_gatt_attr *,void *);
typedef int ble_gatt_mtu_fn(uint16_t,const struct ble_gatt_error *,uint16_t,void *);
int ble_gattc_disc_all_svcs(uint16_t,ble_gatt_disc_svc_fn *,void *);
int ble_gattc_disc_all_chrs(uint16_t,uint16_t,uint16_t,ble_gatt_chr_fn *,void *);
int ble_gattc_disc_all_dscs(uint16_t,uint16_t,uint16_t,ble_gatt_dsc_fn *,void *);
int ble_gattc_write_flat(uint16_t,uint16_t,const void *,uint16_t,ble_gatt_attr_fn *,void *);
int ble_gattc_write_no_rsp_flat(uint16_t,uint16_t,const void *,uint16_t);
int ble_gattc_read(uint16_t,uint16_t,ble_gatt_attr_fn *,void *);
int ble_gattc_exchange_mtu(uint16_t,ble_gatt_mtu_fn *,void *);
uint16_t ble_att_mtu(uint16_t);
int ble_att_set_preferred_mtu(uint16_t);
