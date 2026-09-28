#include "../../esp8684/main/victron_frames.h"
#include "../../esp8684/main/victron_transport.h"
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)

/* Replay representative records from the successful SmartSolar PC session.
 * Identity/serial records are excluded. The PC used 74-byte ATT payloads;
 * ESP peers can use the default 20-byte payload instead. DATA chunks append
 * to the message and only LAST permits parsing, including a final full chunk.
 */
static int replay_smartsolar(size_t chunk_size) {
    static const uint8_t discovery[]={2,0x9f,0,0,1,0,3,1,0xff};
    static const uint8_t ack[]={7,0,3,0};
    static const uint8_t telemetry[]={
        8,3,0x19,0xec,0x8a,0x42,0,0,
        8,3,0x19,0xed,0xbc,0x44,0,0,0,0
    };
    static const uint8_t history[]={
        8,3,0x19,0xec,0x20,0x58,0x20,
        0x8d,0xed,1,0x0c,0x99,0x87,2,0x98,
        0xec,0xed,1,0x0c,0x99,0x87,2,0x98,
        0x3e,0xec,0xff,0xff,0xff,0xff,0xff,0xff,
        0x8c,0xed,1,0x0c,0x99,0x87,2,0x98
    };
    static const uint8_t keepalive[]={8,0,0x18,0x93,0x42,0x10,0x27};
    static const uint8_t mode_off[]={8,3,0x19,0xed,0xab,0x41,0};
    static const uint8_t mode_on[]={8,3,0x19,0xed,0xab,0x41,4};
    static const uint8_t output_on[]={8,3,0x19,0xed,0xa8,0x41,1};
    const struct {const uint8_t *p;size_t n;uint8_t subscribe;uint16_t awaited;} frames[]={
        {discovery,sizeof(discovery),255,0},
        {ack,sizeof(ack),0,0},{ack,sizeof(ack),3,0},
        {telemetry,sizeof(telemetry),255,0},
        {history,sizeof(history),255,0},
        {keepalive,sizeof(keepalive),255,0},
        {mode_off,sizeof(mode_off),255,0xedab},
        {mode_on,sizeof(mode_on),255,0xedab},
        {output_on,sizeof(output_on),255,0xeda8},
        {ack,sizeof(ack),255,0xeda8} // late ACK is not a register rejection
    };
    uint32_t devices=0,subscribed=0;bool rejected=false;
    uint8_t assembled[512];size_t length=0;unsigned values=0;
    for(size_t i=0;i<sizeof(frames)/sizeof(frames[0]);++i) {
        for(size_t at=0;at<frames[i].n;) {
            size_t chunk=frames[i].n-at;
            if(chunk>chunk_size)chunk=chunk_size;
            CHECK(length+chunk<=sizeof(assembled));
            memcpy(assembled+length,frames[i].p+at,chunk);length+=chunk;at+=chunk;
            if(at!=frames[i].n)continue; // DATA; incomplete CBOR is not a message
            CHECK(victron_session_frame(assembled,length,&devices,&subscribed,3,
                                       frames[i].subscribe,frames[i].awaited,&rejected));
            CHECK(!rejected);
            uint8_t value=255;size_t size=0;
            if(victron_find_value(assembled,length,3,frames[i].awaited,&value,1,&size)) {
                CHECK(size==1 && (value==0 || value==1 || value==4));++values;
            }
            length=0;
        }
    }
    CHECK(devices==11 && subscribed==9 && values==3 && length==0);
    return 0;
}

int main(void) {
    CHECK(replay_smartsolar(20)==0);
    CHECK(replay_smartsolar(74)==0);
    /* Every possible split covers multi-byte CBOR lengths and register keys. */
    for(size_t chunk=1;chunk<=74;++chunk)CHECK(replay_smartsolar(chunk)==0);
    victron_flow_t f={0};
    const uint8_t info[]={0,1,0,1,80,20,0},credit[]={0xf9,1};
    CHECK(victron_flow_init(&f,info,sizeof(info)));
    CHECK(!victron_flow_take_tx(&f));
    CHECK(victron_flow_control(&f,credit,sizeof(credit)));
    CHECK(victron_flow_take_tx(&f));
    CHECK(!victron_flow_take_tx(&f));
    CHECK(victron_flow_control(&f,credit,sizeof(credit)));
    CHECK(!victron_flow_control(&f,credit,sizeof(credit))); // overflow, not wraparound
    CHECK(victron_flow_init(&f,info,sizeof(info)));
    for(int i=0;i<31;i++) victron_flow_received(&f);
    CHECK(victron_flow_return_rx(&f)==0);
    victron_flow_received(&f); CHECK(victron_flow_return_rx(&f)==32);
    CHECK(victron_flow_return_rx(&f)==0);
    for(int i=0;i<128;i++) victron_flow_received(&f);
    CHECK(!f.failed && victron_flow_return_rx(&f)==128);
    for(int i=0;i<129;i++) victron_flow_received(&f);
    CHECK(f.failed && !victron_flow_take_tx(&f));
    CHECK(victron_flow_init(&f,info,sizeof(info)));
    CHECK(!victron_flow_control(&f,credit,1));
    CHECK(!victron_flow_init(&f,info,6));
    CHECK(victron_flow_init(&f,info,sizeof(info)));
    const uint8_t error[]={0xf7,3,0};
    CHECK(!victron_flow_control(&f,error,sizeof(error)));
    CHECK(!victron_request_fits(0) && victron_request_fits(20) && !victron_request_fits(21));
    for(unsigned i=0;i<256;i++) {
        CHECK(victron_mppt_expected_output(i)==((i&15)==4?1:(i&15)==0?0:-1));
    }
    uint32_t dev=0,sub=0;bool rejected=false;
    const uint8_t discovery[]={2,0x9f,0,0,1,0,3,1,0xff};
    CHECK(victron_session_frame(discovery,sizeof(discovery),&dev,&sub,3,255,0,&rejected));
    CHECK(dev==11 && sub==0 && !rejected);
    const uint8_t definite[]={2,0x84,0,0,3,1};
    dev=0;CHECK(victron_session_frame(definite,sizeof(definite),&dev,&sub,3,255,0,&rejected));
    CHECK(dev==9);
    for(size_t n=0;n<sizeof(discovery);n++) {
        dev=0; CHECK(!victron_session_frame(discovery,n,&dev,&sub,3,255,0,&rejected));
        CHECK(dev==0); // no partially accepted identity
    }
    const uint8_t ack[]={7,0,3,0}; // Actual SmartSolar reply, also for Subscribe(3).
    CHECK(victron_session_frame(ack,sizeof(ack),&dev,&sub,3,0,0,&rejected) && sub==1);
    CHECK(victron_session_frame(ack,sizeof(ack),&dev,&sub,3,3,0,&rejected) && sub==9);
    sub=0;CHECK(victron_session_frame(ack,sizeof(ack),&dev,&sub,3,255,0,&rejected) && !sub && !rejected);
    const uint8_t nack[]={7,0,3,1};
    CHECK(victron_session_frame(nack,sizeof(nack),&dev,&sub,3,3,0,&rejected) && rejected);
    rejected=false;
    CHECK(victron_session_frame(nack,sizeof(nack),&dev,&sub,3,255,0xedab,&rejected) && !rejected);
    /* An ACK for another active instance cannot complete/fail Subscribe(3). */
    const uint8_t other_ack[]={7,1,3,0},other_nack[]={7,1,3,0x20};
    sub=0;CHECK(victron_session_frame(other_ack,sizeof(other_ack),&dev,&sub,3,3,0,&rejected) && !sub && !rejected);
    CHECK(victron_session_frame(other_nack,sizeof(other_nack),&dev,&sub,3,3,0,&rejected) && !sub && !rejected);
    const uint8_t denied[]={9,3,0x19,0xed,0xab,0x20};
    CHECK(victron_session_frame(denied,sizeof(denied),&dev,&sub,3,255,0xedab,&rejected) && rejected);
    rejected=false;
    CHECK(victron_session_frame(denied,sizeof(denied),&dev,&sub,0,255,0xedab,&rejected) && !rejected);
    const uint8_t on[]={8,3,0x19,0xed,0xab,0x41,4,8,3,0x19,0xed,0xa8,0x41,1};
    uint8_t value[4]={0};size_t size=0;
    CHECK(victron_find_value(on,sizeof(on),3,0xedab,value,sizeof(value),&size) && size==1 && value[0]==4);
    CHECK(victron_find_value(on,sizeof(on),3,0xeda8,value,sizeof(value),&size) && size==1 && value[0]==1);
    CHECK(!victron_find_value(on,sizeof(on),0,0xeda8,value,sizeof(value),&size));
    return 0;
}
