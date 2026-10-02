// Copyright (c) 2026 Naemon team - license: GPL-2.0
// This file is part of the naemon project: https://www.naemon.io
// See LICENSE file in the project root for details.

#ifndef DoubleFormat_h
#define DoubleFormat_h

#include <cstdint>
#include <stdint.h>
#include <string.h>

// snprintf() parses its format string, and unpacks its variadic arguments
// snprintf() does not cache its (input + format) -> (output) . It is however a pure function that can be cached

// Doubles are mostly printed in this code using format "%.10e"
// This cache memoizes the snprintf() of the "%.10e" results. Do not save other snprintf formats to it.

// The caching stragety for double formats follows these principles:
// bounded -> according to the DOUBLE_CACHE_LOG2 constant.
// direct access -> DOUBLE_CACHE_LOG2 sized DoubleCacheEntry array memoizes one value, collisions evict
// exact -> DoubleCacheEntry also saves the bits of original double, so hash collisions are detected.
// thread_local -> livestatus has one thread per connection. multiple connections can not work on the same cache

// quirks with some double values:
// -0.0 and +0.0 have different bit patterns and print differently, so their bit keys and hashes are different, as it should be
// NaN payloads each occupy a slot and all print "nan"

#define DOUBLE_CACHE_LOG2 10
#define DOUBLE_CACHE_SIZE (1 << DOUBLE_CACHE_LOG2)

// The constant represents 2^64 / φ (where φ is the golden ratio, approximately 1.61803398875), specifically capturing the fractional part of the golden ratio scaled to 64 bits.
const uint64_t FIBONACCI_HASHING_MULTIPLER_64_BIT = 0x9E3779B97F4A7C15ULL;

struct DoubleCacheEntry {
    uint64_t bits;
    unsigned char len;
    // "%.10e" never produces more than 18 characters ("-1.2345678901e-308"),
    // 24 bytes always hold the result plus its NUL, and aligns nicely 3 * 8 = 24
    char text[24];
};

extern thread_local DoubleCacheEntry g_double_cache_dot10e[DOUBLE_CACHE_SIZE];

// Cold path: snprintf()s the value, remembers it, returns the text.  In Query.cc
const char *formatDoubleDot10eSlow(double value, int *len);

// Returns the "%.10e" text of value and its length.
// On a hit the text lives in the calling thread's cache and stays valid until that thread calls this function again,
// so callers copy or append the bytes immediately.
static inline const char *formatDoubleDot10e(double value, int *len)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));

    // Fibonacci hashing: multiply by FIBONACCI_HASHING_MULTIPLER_64_BIT and use the top bits.
    // In C++, multiplying two uint64_t can overflow beyond 64 bits, multiplication will however store the lower 64 bits of correct, possibly overflowed, result.
    // Doubles have 1 sign bit, 11 exponent bits, and 52 mantissa bits from high to low.
    // The mantissas bits are low, and they are the ones who typically change the most.
    // After multiplying with FIBONACCI_HASHING_MULTIPLER_64_BIT ~ roughly around 2^63, they shift upwards to higher bits, and lower bits are occupied with pseudo random results, sensitive to changes in mantissa.
    // Shift to the multiplication result right to preserve their information.

    DoubleCacheEntry *e = &g_double_cache_dot10e[
        (size_t)( (bits * FIBONACCI_HASHING_MULTIPLER_64_BIT ) >> (64 - DOUBLE_CACHE_LOG2) )
    ];
    if (e->len != 0 && e->bits == bits) {
        *len = e->len;
        return e->text;
    }
    return formatDoubleDot10eSlow(value, len);
}

#endif
