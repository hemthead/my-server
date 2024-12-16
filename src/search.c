#include <stddef.h>
#include <string.h>

#include <stdio.h>

#include "search.h"

size_t search(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len) {
    if (haystack_len < needle_len)
        return 0; // that's not gonna work!

    const unsigned int p = 101u; // the prime modulus
    const unsigned int b = 256u; // the base

    unsigned int base_offset = 1; // the amount to multiply haystack[i] by to subtract it from the hash
    for (size_t i = 0; i < needle_len - 1; ++i)
        // b^(needle_len); mod by p to keep things small
        base_offset = (base_offset * b) % p;

    // compute the needle's and first window's hashes
    unsigned int n_hash = 0;
    unsigned int h_hash = 0;
    for (size_t i = 0; i < needle_len; ++i) {
        n_hash = (((n_hash * b) % p) + needle[i]) % p;
        h_hash = (((h_hash * b) % p) + haystack[i]) % p;
    };

    // loop over the haystack, rolling and comparing hash each time
    for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
        if (h_hash == n_hash) {
            if (memcmp(haystack+i, needle, needle_len) == 0)
                return i+needle_len;
        }

        // calculate the next hash if there is a next iteration
        if (i < haystack_len - needle_len)
            h_hash = (((h_hash + p) - ((haystack[i] * base_offset) % p)) * b + haystack[i+needle_len]) % p;
    }

    return 0;
}
