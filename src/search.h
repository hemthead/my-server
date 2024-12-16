/* Search a bytearray haystack for a bytearray needle using the Rabin-Karp algorithm.
   `haystack` is a pointer to the beginning of the haystack.
   `haystack_len` is the size of the haystack.
   `needle` is a pointer to the beginning of the needle.
   `needle_len` is the size of the needle.
   Returns the index after the end of the found needle, or 0 when none found. */
size_t search(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len);
