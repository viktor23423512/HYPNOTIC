#include "touchpad_routing.h"
#include <cassert>
#include <cstdio>
int main(){TouchpadRouting r;assert(!r.searchSide(100));r.observe(1,100,100);assert(r.searchSide(100)&&r.searchSide(250100)&&!r.searchSide(250101)&&!r.searchSide(99));r.observe(1,1500,200);assert(!r.searchSide(200));r.setWidth(1000);r.observe(1,499,300);assert(r.searchSide(300));r.observe(1,500,400);assert(!r.searchSide(400));r.reset();assert(!r.searchSide(400));r.setWidth(0);assert(r.width==1000);r.observe(0,50,500);assert(!r.searchSide(500));r.observe(3,50,500);assert(!r.searchSide(500));r.observe(1,1000,500);assert(!r.searchSide(500));puts("PASS native-width split, midpoint, timestamp bounds, no-finger/invalid input, fallback and reset");}
