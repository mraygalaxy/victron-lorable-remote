// Exercise the real manager with the same bounded RUI/LoRaMAC double as its
// existing integration suite. The configured-power gate is stubbed separately from
// the power helper's own tests, so a denied gate must prevent join RF here.
#if !defined(MANUAL_NETWORK_STANDARD)
#define LORABLE_MANUAL_RADIO_TEST 1
#endif
#define main legacy_network_manager_main
#include "network-manager.cpp"
#undef main

static bool allowPower=true;
static unsigned powerChecks;
#if defined(LORABLE_MANUAL_RADIO_TEST)
static bool manualOverride,allowManualRadio=true,busyOnPower;
static unsigned overrideBegins,overrideEnds;
bool loraApplyPower(){
 ++powerChecks;assert(persistentNonce>radioContext.Crypto.DevNonce);
 if(busyOnPower){busyOnPower=false;radioBusy=true;return false;}
 return allowPower;
}
bool loraManualRadioBegin(){
 if(!allowManualRadio||radioBusy||manualOverride)return false;
 manualOverride=true;++overrideBegins;return true;
}
void loraManualRadioEnd(){if(manualOverride){manualOverride=false;++overrideEnds;}}
#endif

static void requestValidation(){
 assert(!networkJoinNow()&&!networkManualJoinPending());
 auto c=configuration();c.networks[0].enabled=c.networks[1].enabled=0;boot(c,100);
 assert(!networkJoinNow()&&!networkJoinNow(4)&&!networkJoinNow(2));
 c=configuration();memset(c.networks[0].appKey,0,16);boot(c,100);
 assert(!networkJoinNow(0));assert(networkJoinNow());tick(0);
 assert(chosenJoin==1&&joinCalls==1&&!networkManualJoinPending());
 c=configuration();c.networks[0].rx2Custom=1;c.networks[0].rx2Frequency=1;boot(c,100);
 assert(!networkJoinNow(0)&&networkJoinNow());tick(0);assert(chosenJoin==1);
 c=configuration();c.loraRegion=255;boot(c,100);assert(!networkJoinNow());
 puts("PASS: manual request validates enabled profile, key, region, RX2 and slot");
}

static void immediatePreemptAndBusy(){
 auto c=configuration();const auto original=c;boot(c,200);
 tick(0);complete(8,false);tick(68);complete(76,true,8);
 assert(networkJoined()&&networkActiveSlot()==1&&networkPreemptRemaining()>0);
 const unsigned oldCalls=joinCalls,oldClears=activationClears;
 const uint16_t oldNonce=radioContext.Crypto.DevNonce,oldReserved=persistentNonce;
 nowSeconds=77;assert(networkJoinNow()&&networkManualJoinPending());
 assert(networkJoined()&&joinCalls==oldCalls&&activationClears==oldClears);
 networkTick(true);assert(networkActiveSlot()==1&&joinCalls==oldCalls&&networkJoined());
 radioBusy=true;tick(78);assert(networkActiveSlot()==1&&activationClears==oldClears);
 radioBusy=false;tick(79);
 assert(networkActiveSlot()==0&&joinCalls==oldCalls+1&&!networkJoined()&&!networkManualJoinPending());
 assert(radioContext.Crypto.DevNonce==oldNonce+1&&persistentNonce==oldReserved);
 assert(memcmp(&c,&original,sizeof(c))==0&&savedJoinNonce[1]==8);
 complete(87,true,60);assert(networkJoined());
 nowSeconds=88;assert(networkJoinNow(0));tick(88);
 assert(!networkJoined()&&joinCalls==oldCalls+2&&radioContext.Crypto.DevNonce==oldNonce+2);
 puts("PASS: manual preempt/same-slot rejoin, TX/radio deferral, unchanged config and nonce reservation");
}

static void outstandingJoinOwnership(){
 auto c=configuration();boot(c,300);tick(0);
 assert(networkJoinNow(1)&&networkManualJoinPending());
 // Even an idle driver past the old watchdog deadline is not a terminal join
 // callback. Preserve the old slot until its nonce is durably attributed.
 tick(1000);assert(joinCalls==1&&networkActiveSlot()==0&&networkManualJoinPending());
 complete(1001,true,72);
 assert(savedJoinNonce[0]==72&&networkActiveSlot()==1&&chosenJoin==1&&joinCalls==2);
 assert(!networkManualJoinPending()&&!networkJoined());
 complete(1009,true,9);assert(savedJoinNonce[1]==9&&savedJoinNonce[0]==72&&networkJoined());
 puts("PASS: queued rejoin consumes previous callback before changing network/JoinNonce ownership");
}

static void queuedManualSendHoldsSession(){
 auto c=configuration();boot(c,350);
 tick(0);complete(8,false);tick(68);complete(76,true,12);
 assert(networkJoined()&&networkActiveSlot()==1);
 const unsigned originalJoins=joinCalls,originalClears=activationClears;
 bool manualConfirmPending=true,statusTxInFlight=true;
 const auto serviceNetwork=[&](uint32_t time){
  nowSeconds=time;
  networkTick(statusTxInFlight||(manualConfirmPending&&networkJoined()));
 };
 // The manual send queues behind a previous TX as backup preemption comes due.
 radioBusy=true;serviceNetwork(908);
 assert(networkPreemptRemaining()==0&&networkJoined()&&networkActiveSlot()==1);
 networkTxComplete(false,true);statusTxInFlight=false;radioBusy=false;
 // Although the MAC is now idle, the queued manual send owns this session.
 serviceNetwork(909);
 assert(networkJoined()&&networkActiveSlot()==1&&joinCalls==originalJoins);
 assert(activationClears==originalClears&&api.lorawan.njs.value==1);
 // Main-loop send acceptance transfers the hint from queued to in-flight.
 manualConfirmPending=false;statusTxInFlight=true;radioBusy=true;
 serviceNetwork(910);
 assert(networkJoined()&&networkActiveSlot()==1&&joinCalls==originalJoins);
 networkTxComplete(false,true);statusTxInFlight=false;radioBusy=false;
 serviceNetwork(918);
 assert(networkActiveSlot()==0&&!networkJoined()&&joinCalls==originalJoins+1);
 assert(chosenJoin==0&&activationClears>originalClears);
 puts("PASS: queued manual send preserves due-preempt backup session until TX completion, then normal preempt resumes");
}

static void retainedTtnBudgets(){
 auto c=configuration();c.networks[0].kind=1;boot(c,400);
#if defined(LORABLE_MANUAL_RADIO_TEST)
 tick(0);complete(8,true,80);
 for(unsigned i=1;i<7;++i){
  nowSeconds=i*10;assert(networkJoinNow());tick(i*10);
  assert(joinCalls==i+1&&!networkManualJoinPending()&&manualOverride);
  assert(policy.joinsToday[0]==i+1&&policy.lastAttemptAt[0]==i*10);
  complete(i*10+8,true,80+i);assert(!manualOverride&&networkJoined());
 }
 // Subsequent automatic retries are not exempt: accepted manual requests
 // remain counted, so exceeding the budget blocks ordinary RF until rollover.
 networkTxComplete(true,false);networkTxComplete(true,false);
 assert(networkActiveSlot()==1);tick(128);complete(136,true,20);tick(1028);
 assert(joinCalls==8&&networkActiveSlot()==1&&networkState()==NetworkPolicy::WAITING);
 assert(policy.joinsToday[0]==7&&policy.lastAttemptAt[0]==60);
 puts("PASS: manual-radio test variant joins bypass TTN limits once while preserving automatic join accounting");
#else
 tick(0);complete(8,true,80);assert(networkJoinNow());tick(9);
 assert(joinCalls==1&&networkManualJoinPending()&&!networkJoined()&&api.lorawan.njs.value==0);
 assert(networkState()==NetworkPolicy::BUDGET_WAIT&&networkActiveSlot()==0);
 assert(networkRetryRemaining()==3591&&policy.joinsToday[0]==1);
 assert(networkJoinNow());tick(3599);assert(joinCalls==1&&policy.joinsToday[0]==1);
 tick(3600);assert(joinCalls==2&&!networkManualJoinPending());complete(3608,true,81);
 for(unsigned i=2;i<6;++i){
  nowSeconds=i*3600;assert(networkJoinNow());tick(i*3600);
  assert(joinCalls==i+1);complete(i*3600+8,true,80+i);
 }
 nowSeconds=21600;assert(networkJoinNow());tick(21600);
 assert(joinCalls==6&&policy.joinsToday[0]==6&&networkState()==NetworkPolicy::BUDGET_WAIT);
 assert(networkManualJoinPending()&&networkRetryRemaining()==64800&&networkActiveSlot()==0);
 assert(networkJoinNow());tick(86399);assert(joinCalls==6&&policy.joinsToday[0]==6);
 tick(86400);assert(joinCalls==7&&policy.joinsToday[0]==1&&!networkManualJoinPending());
 puts("PASS: manual requests retain TTN hourly cooldown/daily join budget without fallback or reset");
#endif
}

static void driverAndSecurityGates(){
 auto c=configuration();c.networks[1].enabled=0;boot(c,500);
 assert(networkJoinRequestCode()==-1);
 rejectJoin=true;assert(networkJoinNow());tick(0);
#if defined(LORABLE_MANUAL_RADIO_TEST)
 assert(joinRequests==1&&joinCalls==0&&!networkManualJoinPending()&&!manualOverride&&policy.joinsToday[0]==0);
 assert(networkJoinRequestCode()==LORAMAC_STATUS_ERROR);
 // A new explicit click is a new one-shot request; no automatic exempt retry.
 tick(1);assert(joinRequests==1);rejectJoin=false;assert(networkJoinNow());tick(1);
 assert(joinCalls==1&&!networkManualJoinPending()&&manualOverride&&policy.joinsToday[0]==1);
 assert(networkJoinRequestCode()==LORAMAC_STATUS_OK);complete(9,false);assert(!manualOverride);
 boot(c,550);allowManualRadio=false;assert(networkJoinNow());tick(0);
 assert(joinCalls==0&&!networkManualJoinPending()&&networkJoinRequestCode()==LORAMAC_STATUS_ERROR);
 allowManualRadio=true;tick(1);assert(joinCalls==0);assert(networkJoinNow());tick(1);
 assert(joinCalls==1&&!networkManualJoinPending());complete(9,true,90);assert(!manualOverride);
#else
 assert(joinRequests==1&&joinCalls==0&&networkManualJoinPending()&&policy.joinsToday[0]==0);
 assert(networkJoinNow());rejectJoin=false;tick(59);assert(joinRequests==1);
 tick(60);assert(joinCalls==1&&!networkManualJoinPending()&&policy.joinsToday[0]==1);
#endif
#if defined(LORABLE_MANUAL_RADIO_TEST)
 boot(c,600);allowPower=false;const unsigned checks=powerChecks;
 assert(networkJoinNow());tick(0);
 assert(powerChecks==checks+1&&joinRequests==0&&!networkManualJoinPending());
 assert(networkJoinRequestCode()==LORAMAC_STATUS_ERROR);
 tick(1);assert(powerChecks==checks+1&&joinRequests==0); // no every-loop retry
 allowPower=true;assert(networkJoinNow());tick(1);assert(joinCalls==1&&!networkManualJoinPending());
 boot(c,650);busyOnPower=true;assert(networkJoinNow());tick(0);
 assert(joinCalls==0&&networkManualJoinPending()&&radioBusy);
 tick(1);assert(joinCalls==0&&networkManualJoinPending());
 radioBusy=false;tick(2);assert(joinCalls==1&&!networkManualJoinPending());
#endif
 boot(c,700);failReserve=true;assert(networkJoinNow());tick(0);
 assert(joinCalls==0&&networkState()==NetworkPolicy::FAULT&&!networkManualJoinPending()&&!networkJoinNow());
 boot(c,65535);assert(networkJoinNow());tick(0);
 assert(joinCalls==0&&networkState()==NetworkPolicy::FAULT&&!networkJoinNow());
 boot(c,800);tick(0);assert(networkJoinNow(0));failLedger=true;complete(8,true,90);
 assert(joinCalls==1&&networkState()==NetworkPolicy::FAULT&&!networkManualJoinPending()&&!networkJoinNow());
#if defined(LORABLE_MANUAL_RADIO_TEST)
 assert(!manualOverride&&overrideBegins==overrideEnds);
#endif
 puts("PASS: duty-cycle/driver defer, power no-RF gate, nonce exhaustion/reservation/ledger fail closed");
}

int main(){
 requestValidation();immediatePreemptAndBusy();outstandingJoinOwnership();
 queuedManualSendHoldsSession();retainedTtnBudgets();driverAndSecurityGates();
}
