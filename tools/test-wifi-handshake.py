"""Compile the actual UART WiFi handlers against a tiny host runtime."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


def function(source, name):
    # Selected handlers contain no braces in strings/comments.
    start = source.index(name + '(') if name + '(' in source else source.index(name + ' (')
    start = source.rfind('\n', 0, start) + 1
    opening = source.index('{', start)
    level = 1
    end = opening + 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


class WifiHandshake(unittest.TestCase):
    def test_real_handlers(self):
        app = (ROOT / 'esp8684/main/app_main.c').read_text()
        protocol = (ROOT / 'esp8684/main/protocol.c').read_text()
        policy = (ROOT / 'esp8684/main/wifi_client_policy.h').as_posix()
        source = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
enum {ESP_OK=0,ESP_ERR_NOT_FOUND=1,ESP_ERR_INVALID_STATE=2,ESP_ERR_INVALID_ARG=3,ESP_ERR_INVALID_SIZE=4};
enum {pdTRUE=1};
#define pdMS_TO_TICKS(x) (x)
typedef struct {char type[21];uint16_t id;char payload[385];} protocol_frame_t;
typedef struct {uint8_t mode;uint16_t delay_s,timeout_s;char ssid[33],password[64];} portal_wifi_config_t;
static struct {
    bool portal_requested,portal_restart;
    int64_t portal_deadline_us;
    char portal_ssid[33],portal_password[64];
    portal_wifi_config_t wifi;
    uint16_t wifi_config_id;
} application_state;
static int state_lock,manager_task;
static bool last_ok;
static char last_code[64];
static int xSemaphoreTake(int lock,int ticks){(void)lock;(void)ticks;return pdTRUE;}
static void xSemaphoreGive(int lock){(void)lock;}
static void xTaskNotifyGive(int task){(void)task;}
static int64_t esp_timer_get_time(void){return 1000;}
static void send_result(uint16_t id,bool ok,const char *code){(void)id;last_ok=ok;snprintf(last_code,sizeof(last_code),"%s",code);}
'''
        source += '#include "' + policy + '"\n'
        for name in ('hex_value', 'protocol_secure_zero', 'protocol_form_get'):
            source += function(protocol, name)
        for name in ('parse_u32', 'visible_without_space', 'valid_ssid', 'derive_ssid',
                     'get_required_u32', 'handle_portal_start', 'handle_wifi_config'):
            source += function(app, name)
        source += r'''
#include <stdatomic.h>
#define portMAX_DELAY 999
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#define AF_INET 2
#define WIFI_MODE_APSTA 3
typedef unsigned socklen_t;
typedef struct {int unused;} httpd_req_t;
struct sockaddr {int unused;};
struct sockaddr_in {int sin_family;struct {uint32_t s_addr;} sin_addr;};
typedef struct {struct {uint32_t addr;} ip;} esp_netif_ip_info_t;
static int radio_policy_lock=1;
static void *access_point_netif=(void*)1;
static void *station_netif=(void*)2;
static wifi_client_policy_t client_policy;
static atomic_bool upload_entry_hold,wifi_started,station_connected,station_disconnected;
static uint32_t mock_socket_ip;
static uint32_t mock_station_ip=0xc0a80430;
static unsigned disconnect_calls;
static int httpd_req_to_sockfd(httpd_req_t *r){(void)r;return 1;}
static int getsockname(int fd,struct sockaddr *address,socklen_t *size){
    (void)fd;(void)size;struct sockaddr_in *a=(struct sockaddr_in*)address;
    a->sin_family=AF_INET;a->sin_addr.s_addr=mock_socket_ip;return 0;
}
static int esp_netif_get_ip_info(void *netif,esp_netif_ip_info_t *info){
    info->ip.addr=netif==station_netif?mock_station_ip:0xc0a80401;return ESP_OK;
}
static void esp_wifi_disconnect(void){disconnect_calls++;}
'''
        source += function((ROOT / 'esp8684/main/portal.c').read_text(), 'portal_wifi_prepare_upload')
        source += r'''
static void configure(uint16_t id,const char *payload){
    protocol_frame_t frame={.id=id};snprintf(frame.payload,sizeof(frame.payload),"%s",payload);
    handle_wifi_config(&frame);
}
static void start(const char *reference){
    protocol_frame_t frame={.id=100};
    snprintf(frame.payload,sizeof(frame.payload),"seconds=3600&ssid=Remote&password=123456789012%s",reference);
    handle_portal_start(&frame);
}
int main(void){
    start("&wifi_cfg=9");assert(!last_ok&&!application_state.portal_requested);
    assert(!strcmp(last_code,"wifi_config_needed"));
    configure(9,"mode=2&delay=90&timeout=60&ssid=&password=");
    assert(last_ok&&application_state.wifi.mode==2&&application_state.wifi_config_id==9);
    start("&wifi_cfg=9");assert(last_ok&&application_state.portal_requested&&application_state.wifi.mode==2);
    application_state.portal_requested=false;
    start("&wifi_cfg=8");assert(!last_ok&&!application_state.portal_requested);
    start("&wifi_cfg=0");assert(!last_ok&&!application_state.portal_requested);
    start("&wifi_cfg=65536");assert(!last_ok&&!application_state.portal_requested);
    start("&wifi_cfg=9&wifi_cfg=9");assert(!last_ok&&!application_state.portal_requested);
    configure(10,"mode=1&delay=0&timeout=10&ssid=Boat+WiFi&password=space+pass%26");
    assert(last_ok&&application_state.wifi_config_id==10);
    assert(!strcmp(application_state.wifi.ssid,"Boat WiFi"));
    assert(!strcmp(application_state.wifi.password,"space pass&"));
    start("&wifi_cfg=9");assert(!last_ok&&!application_state.portal_requested);
    start("&wifi_cfg=10");assert(last_ok&&application_state.portal_requested);
    application_state.portal_requested=false;
    configure(11,"mode=1&delay=0&timeout=10&ssid=Boat&password=short");
    assert(!last_ok&&application_state.wifi_config_id==10);
    start("&wifi_cfg=11");assert(!last_ok&&!application_state.portal_requested);
    configure(0,"mode=2&delay=90&timeout=60&ssid=&password=");assert(!last_ok);
    configure(65535,"mode=0&delay=600&timeout=300&ssid=&password=");assert(last_ok);
    start("&wifi_cfg=65535");assert(last_ok&&application_state.portal_requested);
    /* Lost START/replies can be retried without treating rejection as success. */
    start("&wifi_cfg=65535");assert(last_ok);
    memset(&application_state,0,sizeof(application_state));
    start("");assert(last_ok&&application_state.wifi.mode==0); /* legacy AP host */
    httpd_req_t request={0};wifi_started=true;client_policy.mode=WIFI_CLIENT_ROUTER;
    client_policy.ap=false;station_connected=true;mock_socket_ip=0xc0a80430;
    portal_wifi_prepare_upload(&request);
    assert(upload_entry_hold&&!disconnect_calls&&station_connected); /* STA upload route. */
    upload_entry_hold=false;mock_socket_ip=0xc0a80401;client_policy.ap=true;
    portal_wifi_prepare_upload(&request);
    assert(upload_entry_hold&&disconnect_calls==1&&!station_connected&&station_disconnected);
    assert(client_policy.ap); /* AP upload wins over a just-arrived GOT_IP. */
    disconnect_calls=0;upload_entry_hold=false;client_policy.ap=false;station_connected=true;
    portal_wifi_prepare_upload(&request);
    assert(!disconnect_calls&&!client_policy.ap&&station_connected); /* DHCP assigned STA192.168.4.1; AP inactive. */
    mock_station_ip=0xc0a80401;client_policy.ap=true;
    portal_wifi_prepare_upload(&request);
    assert(!disconnect_calls&&client_policy.ap&&station_connected); /* Both active with same IP: preserve STA. */
    mock_station_ip=0xc0a80430;client_policy.ap=false;
    disconnect_calls=0;mock_socket_ip=0x7f000001;station_connected=true;
    portal_wifi_prepare_upload(&request);
    assert(!disconnect_calls&&station_connected); /* USB preserves established STA. */
    station_connected=false;
    portal_wifi_prepare_upload(&request);
    assert(disconnect_calls==1&&station_disconnected); /* USB cancels only unfinished association/DHCP. */
    disconnect_calls=0;wifi_started=false;client_policy.mode=WIFI_CLIENT_OFF;
    portal_wifi_prepare_upload(&request);assert(!disconnect_calls);
    puts("WiFi UART handlers: missing/stale/malformed/duplicate refs, off, credentials, retry and legacy passed");
    puts("WiFi OTA entry: STA/AP/USB route preservation, GOT_IP race, pending attempt cancellation and off passed");
    return 0;
}
'''
        compiler = shutil.which('cc') or shutil.which('gcc')
        zig = shutil.which('zig') or os.environ.get('ZIG')
        if not zig and os.name == 'nt':
            candidate = Path(os.environ['USERPROFILE']) / 'Desktop/Victron_LoRaBLE_Remote/research/ble-venv/Lib/site-packages/ziglang/zig.exe'
            if candidate.exists():
                zig = str(candidate)
        command = [compiler] if compiler else [zig, 'cc'] if zig else None
        if not command:
            raise RuntimeError('A host C compiler or Zig is required')
        with tempfile.TemporaryDirectory(prefix='lorable-wifi-handshake-') as directory:
            directory = Path(directory)
            fixture = directory / 'handlers.c'
            fixture.write_text(source)
            executable = directory / 'handlers.exe'
            result = subprocess.run([*command, '-std=c11', '-Wall', '-Wextra', '-Werror',
                                     str(fixture), '-o', str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout.strip())


if __name__ == '__main__':
    unittest.main()
