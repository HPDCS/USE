#pragma once
#ifndef ATOMIC_16_B_H
#define ATOMIC_16_B_H
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct _uint128_atomic {
    volatile uint64_t low;
    volatile uint64_t high;
} uint128_atomic;

bool cmpexch_weak_relaxed(uint128_atomic *atomic,uint128_atomic *expected,uint128_atomic desired);
uint128_atomic load_relaxed(uint128_atomic const *atomic);
void store_relaxed(uint128_atomic *atomic, uint128_atomic val);
#endif
