#include "network_manager.h"
#include "network_policy.h"
#include "network_nonce_store.h"
#include "nonce_reservation.h"
#include "region_profile.h"
#include "activity_log.h"
#include "lora_power.h"
#include <board.h>
extern "C" {
#include "LoRaMac.h"
#include "service_lora.h"
}
namespace {
NetworkPolicy policy;
const RuntimeConfig *settings;
uint8_t identity[8];
volatile bool resultPending;
volatile int32_t joinResult;
bool activated;
bool joinInFlight;
bool manualRadioActive;
uint8_t manualJoinSlot=255;
bool manualJoinSelected;
int32_t lastJoinRequestCode=-1;
uint16_t reservedNonceEnd;

uint8_t manualRank(uint8_t requestedSlot){
    if(!settings||!regionProfile(settings->loraRegion))return 255;
    for(uint8_t r=0;r<MAX_NETWORKS;++r){
        const uint8_t s=settings->networkOrder[r];
        if(s>=MAX_NETWORKS||(requestedSlot!=255&&s!=requestedSlot))continue;
        const NetworkProfile &p=settings->networks[s];
        if(p.enabled&&p.kind<=1&&networkHasKey(p)&&
           (!p.rx2Custom||validRx2(settings->loraRegion,p.rx2Frequency,p.rx2DataRate)))return r;
    }
    return 255;
}
LoRaMacNvmData_t *contexts(){MibRequestConfirm_t m={};m.Type=MIB_NVM_CTXS;
 return LoRaMacMibGetRequestConfirm(&m)==LORAMAC_STATUS_OK?m.Param.Contexts:nullptr;}
bool prepare(const NetworkProfile &p){
    // No RUI persistent credential setters here: selection is RAM state, not a
    // rewrite of settings/keys on every fallback. The common radio region stays.
    MibRequestConfirm_t m={};m.Type=MIB_DEVICE_CLASS;m.Param.Class=CLASS_A;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    m.Type=MIB_NETWORK_ACTIVATION;m.Param.NetworkActivation=ACTIVATION_TYPE_NONE;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    m.Type=MIB_DEV_EUI;m.Param.DevEui=identity;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    m.Type=MIB_JOIN_EUI;m.Param.JoinEui=(uint8_t *)p.joinEui;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    m.Type=MIB_APP_KEY;m.Param.AppKey=(uint8_t *)p.appKey;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    m.Type=MIB_NWK_KEY;m.Param.NwkKey=(uint8_t *)p.appKey;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    const RegionProfile *region=regionProfile(settings->loraRegion);
    m.Type=MIB_RX2_DEFAULT_CHANNEL;
    m.Param.Rx2DefaultChannel.Frequency=p.rx2Custom?p.rx2Frequency:region->rx2Frequency;
    m.Param.Rx2DefaultChannel.Datarate=p.rx2Custom?p.rx2DataRate:region->rx2DataRate;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    m.Type=MIB_RXC_DEFAULT_CHANNEL;
    if(LoRaMacMibSetRequestConfirm(&m)!=LORAMAC_STATUS_OK)return false;
    LoRaMacNvmData_t *n=contexts();uint32_t nonce;
    if(!n||!networkNonceLoad(identity,p.joinEui,p.appKey,nonce)||n->Crypto.DevNonce==65535){policy.state=NetworkPolicy::FAULT;return false;}
    // Restore only this network's JoinNonce, never another server's counter.
    n->Crypto.JoinNonce=nonce;
    // Reserve BEFORE RF. Runtime switches/retries do not reset this global
    // high-water mark. One NVM write per 16 attempts, not one erase per retry.
    if(n->Crypto.DevNonce>=reservedNonceEnd){
        const uint16_t end=nextNonceReservation(n->Crypto.DevNonce);
        if(service_lora_set_DevNonce(end)!=0){policy.state=NetworkPolicy::FAULT;return false;}
        reservedNonceEnd=end;
    }
    return loraApplyPower();
}
}
void networkBegin(const RuntimeConfig &config,const uint8_t devEui[8]){
    settings=&config;memcpy(identity,devEui,8);resultPending=false;activated=false;
    if(manualRadioActive)loraManualRadioEnd();
    manualRadioActive=false;lastJoinRequestCode=-1;
    joinInFlight=false;manualJoinSlot=255;manualJoinSelected=false;
    // Disable RUI's autonomous timer; only the policy controls retries.
    api.lorawan.join(0,0,60,0);
    reservedNonceEnd=service_lora_get_DevNonce();
    policy.begin(settings->networks,settings->networkOrder,activitySeconds());
}
bool networkJoinNow(uint8_t slot){
    if(policy.state==NetworkPolicy::FAULT)return false;
    const uint8_t r=manualRank(slot);if(r==255)return false;
    const uint8_t selected=settings->networkOrder[r];
    // Repeated clicks coalesce without resetting a budget/defer deadline.
    if(manualJoinSlot!=selected){manualJoinSlot=selected;manualJoinSelected=false;}
    return true;
}
bool networkManualJoinPending(){return settings&&manualJoinSlot!=255;}
int32_t networkJoinRequestCode(){return lastJoinRequestCode;}
void networkJoinResult(int32_t result){joinResult=result;resultPending=true;}
void networkTick(bool txBusy){
    if(!settings)return;const uint32_t now=activitySeconds();
    if(resultPending){
        const int32_t result=joinResult;resultPending=false;
        if(joinInFlight&&policy.state==NetworkPolicy::JOINING){
            joinInFlight=false;
            if(manualRadioActive){loraManualRadioEnd();manualRadioActive=false;}
            if(result==0){
                const NetworkProfile &p=settings->networks[policy.slot];LoRaMacNvmData_t *n=contexts();
                if(!n||!networkNonceSave(identity,p.joinEui,p.appKey,n->Crypto.JoinNonce)){
                    policy.state=NetworkPolicy::FAULT;activated=false;activityAdd(24,policy.slot+1);
                }else{policy.joined(now);activated=true;activityAdd(21,policy.slot+1);}
            }else{activated=false;activityAdd(22,policy.slot+1);policy.failed(settings->networks,settings->networkOrder,now);}
        }
    }
    // A terminal callback belongs to the current profile, even if the driver
    // briefly reports idle. Never replace that profile before consuming it.
    // A lost callback intentionally fails closed instead of risking a late
    // result being saved in a different network's JoinNonce ledger.
    const bool busy=txBusy||LoRaMacIsBusy()||joinInFlight;
    if(activated&&policy.state==NetworkPolicy::ONLINE&&!api.lorawan.njs.get()) {
        activated=false;policy.failed(settings->networks,settings->networkOrder,now);
    }
    if(policy.state==NetworkPolicy::JOINING&&!busy&&(int32_t)(now-policy.nextAt)>=0){
        activated=false;policy.failed(settings->networks,settings->networkOrder,now);
    }
    if(policy.state==NetworkPolicy::FAULT){manualJoinSlot=255;manualJoinSelected=false;return;}
    if(busy)return;
    if(manualJoinSlot!=255){
        const uint8_t r=manualRank(manualJoinSlot);
        if(r==255){manualJoinSlot=255;manualJoinSelected=false;return;}
        if(!manualJoinSelected){
            // Clear the old session only after TX/join callbacks have finished.
            // The request is not a configuration write or a policy restart:
            // all per-profile budget/cooldown and nonce counters are retained.
            activated=false;
            MibRequestConfirm_t m={};m.Type=MIB_NETWORK_ACTIVATION;
            m.Param.NetworkActivation=ACTIVATION_TYPE_NONE;
            const LoRaMacStatus_t clearResult=LoRaMacMibSetRequestConfirm(&m);
            if(clearResult!=LORAMAC_STATUS_OK){
#if defined(LORABLE_MANUAL_RADIO_TEST)
                if(!LoRaMacIsBusy()){
                    manualJoinSlot=255;manualJoinSelected=false;
                    lastJoinRequestCode=(int32_t)clearResult;policy.deferred(now);
                }
#endif
                return;
            }
            policy.probing=false;
            policy.select(settings->networks,settings->networkOrder,r,now);
            manualJoinSelected=true;activityAdd(23,policy.slot+1);
        }
    }else{
        const uint8_t previous=policy.slot;const bool wasOnline=policy.state==NetworkPolicy::ONLINE;
        policy.tick(settings->networks,settings->networkOrder,now,false);
        if(previous!=policy.slot||(wasOnline&&policy.state!=NetworkPolicy::ONLINE)){activated=false;activityAdd(23,policy.slot+1);}
    }
#if defined(LORABLE_MANUAL_RADIO_TEST)
    const bool manualBypass=manualJoinSlot!=255;
#else
    const bool manualBypass=false;
#endif
    if(!manualBypass&&!policy.ready(settings->networks,now)){
        if(manualJoinSlot==255&&policy.state==NetworkPolicy::BUDGET_WAIT&&(policy.probing||(policy.rank==policy.first(settings->networks,settings->networkOrder)&&
            policy.first(settings->networks,settings->networkOrder,policy.rank+1)!=255)))
            policy.failed(settings->networks,settings->networkOrder,now);
        return;
    }
    activated=false;
    if(!prepare(settings->networks[policy.slot])){
        // A physical busy race remains queued; all other manual preparation
        // failures consume this one-shot just like a rejected MLME request.
        if(manualBypass&&(policy.state==NetworkPolicy::FAULT||!LoRaMacIsBusy())){
            manualJoinSlot=255;manualJoinSelected=false;lastJoinRequestCode=LORAMAC_STATUS_ERROR;
        }
        if(policy.state!=NetworkPolicy::FAULT)policy.deferred(now);
        else {manualJoinSlot=255;manualJoinSelected=false;}
        return;
    }
    // Use the bundled RUI 4.2.4 LoRaMAC entry point: the RUI join wrapper would
    // replace our reserved high-water mark with the just-consumed value.
    // RUI's normal MLME callbacks and radio remain. Automatic attempts retain
    // regional timing checks; the test-only manual exception is scoped below.
    MlmeReq_t join={};join.Type=MLME_JOIN;join.Req.Join.NetworkActivation=ACTIVATION_TYPE_OTAA;
    join.Req.Join.Datarate=regionProfile(settings->loraRegion)->uplinkDataRate;
    if(manualBypass){
        if(!loraManualRadioBegin()){
            if(!LoRaMacIsBusy()){
                manualJoinSlot=255;manualJoinSelected=false;lastJoinRequestCode=LORAMAC_STATUS_ERROR;
            }
            policy.deferred(now);return;
        }
        manualRadioActive=true;
        // Exactly one explicitly requested attempt bypasses automatic timing.
        // Even a rejected MLME request consumes it; later retries are ordinary.
        manualJoinSlot=255;manualJoinSelected=false;
    }
    joinInFlight=true;
    lastJoinRequestCode=(int32_t)LoRaMacMlmeRequest(&join);
    if(lastJoinRequestCode!=LORAMAC_STATUS_OK){
        joinInFlight=false;policy.deferred(now);
        if(manualRadioActive){loraManualRadioEnd();manualRadioActive=false;}
    }
    else {
        policy.started(now);manualJoinSlot=255;manualJoinSelected=false;
        activityAdd(20,policy.slot+1);
    }
}
void networkTxComplete(bool healthProbe,bool acknowledged){
    if(!settings||!healthProbe||policy.state!=NetworkPolicy::ONLINE)return;
    policy.health(acknowledged,activitySeconds());activityAdd(acknowledged?25:26,policy.slot+1);
    if(policy.failures>=2){activated=false;policy.failed(settings->networks,settings->networkOrder,activitySeconds());}
}
void networkDownlinkReceived(){if(settings&&policy.state==NetworkPolicy::ONLINE)policy.health(true,activitySeconds());}
bool networkJoined(){return settings&&activated&&policy.state==NetworkPolicy::ONLINE&&api.lorawan.njs.get()!=0;}
bool networkHealthDue(){return settings&&policy.healthDue(settings->networks,activitySeconds(),settings->networkHealthMinutes);}
uint8_t networkActiveSlot(){return settings?policy.slot:255;}
uint8_t networkState(){return settings?policy.state:NetworkPolicy::DISABLED;}
uint8_t networkMissedChecks(){return policy.failures;}
uint32_t networkPreemptRemaining(){return settings?policy.remaining(settings->networks,settings->networkOrder,activitySeconds()):0;}
uint32_t networkRetryRemaining(){return (policy.state==NetworkPolicy::WAITING||policy.state==NetworkPolicy::BUDGET_WAIT)&&
    (int32_t)(policy.nextAt-activitySeconds())>0?policy.nextAt-activitySeconds():0;}
uint32_t networkStatusIntervalMinutes(){if(!settings)return 240;const uint32_t minutes=settings->statusIntervalMinutes;
 return policy.slot<MAX_NETWORKS&&settings->networks[policy.slot].kind==1&&minutes<240?240:minutes;}
