#include <assert.h>
#include <stdio.h>
#include "../../stm32/input_timing.h"
int main(){
    InputActionTimer t;
    assert(!t.take(0,true)); // Boot is not an edge.
    t.edge(10,true,0,0);assert(t.take(10,true));assert(!t.take(100000,true));
    t.edge(100,true,5,10);assert(!t.take(5099,true));assert(!t.take(15099,true));
    assert(t.take(15100,true));assert(!t.take(15101,true));
    t.edge(100,true,5,10);assert(!t.take(1000,false));assert(!t.take(15100,true));
    t.edge(100,true,5,10);t.edge(1000,false,2,3);
    assert(!t.take(5999,false));assert(t.take(6000,false));assert(!t.take(15100,true));
    t.edge(0xfffffff0u,true,1,1);assert(!t.take(100,true));
    assert(!t.take(1983,true));assert(t.take(1984,true));
    t.edge(0,false,3600,3600);assert(!t.take(7199999,false));assert(t.take(7200000,false));
    // Delay alone and minimum duration alone have the same one-shot boundary.
    t.edge(0,true,0,2);assert(!t.take(1999,true));assert(t.take(2000,true));
    t.edge(0,false,2,0);assert(!t.take(1999,false));assert(t.take(2000,false));
    puts("PASS: edge zero/immediate, hold+delay, cancellation, one-shot, opposite replacement, uptime wrap and bounds");
}
