// Copyright (c) 2026 Naemon team - license: GPL-2.0
// This file is part of the naemon project: https://www.naemon.io
// See LICENSE file in the project root for details.

#ifndef NumberFormat_h
#define NumberFormat_h

#include <cstdint>
#include <stdint.h>
#include <string.h>
#include <time.h>

// Numbers are the bulk of every answer, and every would use snprintf() without caching the variadic arguments and format
// snprintf() does not cache its (input + format) -> (output) . It is however a pure function that can be cached

// These helpers write the digits backwards, least significant first, ending at end and walking towards the front of the caller's buffer:
// The result is therefore always the tail [returned pointer, end) of the buffer and no length has to be known before converting.

// Writes the decimal digits of value with the last digit at end[-1]
// Returns a pointer to the first digit.
// It writes the characters backwards starting from the end pointer, requires an adequately sized buffer
static inline char *formatUnsignedBackward(char *end, uint64_t value)
{
    do {
        *--end = (char)('0' + (value % 10));
        value /= 10;
    } while (value);
    return end;
}

// Writes the decimal digits of value, and '-' only for negative values, with the last digit at end[-1]
// Returns a pointer to the first digit, or '-' if the value is negative
// It writes the characters backwards starting from the end pointer, requires an adequately sized buffer
static inline char *formatIntegerBackward(char *end, int64_t value)
{
    if (value < 0) {
        // INT64_MIN cannot be negated, because its magnitude does not fit in an int64_t;
        // bit trick: adding 1 before negating and adding the 1 back in unsigned arithmetic produces the right magnitude (absolute value) for every input, positive or not.
        char *p = formatUnsignedBackward(end, (uint64_t)-(value + 1) + 1);
        *--p = '-';
        return p;
    }
    return formatUnsignedBackward(end, (uint64_t)value);
}

// snprintf("%lu.%06lu") produces such a format for a normal timeval:
// "<seconds>.<microseconds zero padded to six digits>"
// This function formats it the same way, but faster without using snprintf()
// Writes at most 20 + 1 + 6 = 27 bytes at buf and returns the length.
// Only callers that agree on seconds >= 0 and 0 <= usec < 1000000 may use this
static inline int formatTimeVal(char *buf, time_t seconds, long usec)
{
    // time_t is platform dependent, but maximum of 64 bits -> max 20 chars in decimal form. use 8 * 3 = 24 for alignment
    char tmp_seconds[24];
    char *end = tmp_seconds + sizeof(tmp_seconds);
    char *p = formatUnsignedBackward(end, (uint64_t)seconds);
    size_t n = (size_t)(end - p);

    memcpy(buf, p, n);
    buf[n++] = '.';

    // Usec , microseconds takes a value between (0,1000000] . Has at most 6 digits
    // Uses for loop without breaks, and writes '0' as padding if necessarry.
    for (int i = 5; i >= 0; i--) {
        buf[n + i] = (char)('0' + (usec % 10));
        usec /= 10;
    }
    return (int)(n + 6);
}

// Doubles
// Doubles are mostly printed using format "%.10e"
// This is what the cache is intended for. Do not use it for other snprintf formats.
// The rounding done while formatting has to match what the snprintf() should do.
// So, this memoizes the snprintf() results.

// The caching stragety for double formats follows these principles:
// bounded -> according to the DOUBLE_CACHE_LOG2 constant.
// direct access -> DOUBLE_CACHE_LOG2 sized DoubleCacheEntry array memoizes one value, collisions evict
// exact -> DoubleCacheEntry also saves the bits of the value,
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
    // The mantissa's low bits are the ones that change from value to value, so
    // the low bits of the product make poor slot numbers.
    DoubleCacheEntry *e = &g_double_cache_dot10e[(size_t)(
            (bits * FIBONACCI_HASHING_MULTIPLER_64_BIT ) >> (64 - DOUBLE_CACHE_LOG2)
        )];
    if (e->len != 0 && e->bits == bits) {
        *len = e->len;
        return e->text;
    }
    return formatDoubleDot10eSlow(value, len);
}

#endif
