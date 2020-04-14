#include "math.h"
#include "drivers/serial.h"

int64_t abs(int64_t in) {
    /* If input is less than 0, return the opposite of the input, otherwise return input */
    if (in < 0) return -in;
    return in;
}

/* MurmurHash2 algo */
static uint64_t psuedo_random(const void *key, uint64_t len, uint64_t seed) {
    // Some constants that happen to work well
	const uint64_t m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value
	uint64_t h = seed ^ len;

	// Mix 4 bytes at a time into the hash
	const unsigned char *data = (const unsigned char *) key;

	while(len >= 8) {
		uint64_t k = *(uint64_t *) data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 8;
		len -= 8;
	}

	// Handle the last few bytes of the input array
	switch(len) {
        case 7: h ^= (uint64_t) data[6] << 48; break;
        case 6: h ^= (uint64_t) data[5] << 40; break;
        case 5: h ^= (uint64_t) data[4] << 32; break;
        case 4: h ^= (uint64_t) data[3] << 24; break;
	    case 3: h ^= (uint64_t) data[2] << 16; break;
	    case 2: h ^= (uint64_t) data[1] << 8; break;
	    case 1: h ^= (uint64_t) data[0]; break;
        default:
	        h *= m; break;
	};

	/* Do a few final mixes of the hash to ensure the last few
	   bytes are well-incorporated. */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}


uint64_t current_seed = 0x464ef75ff3e552af;

/* The truely epic random generator */
uint64_t random(uint64_t max) {
    current_seed = psuedo_random((void *) 0xFFFFFFFF80100000, 400, current_seed);
    return current_seed % (max + 1);
}