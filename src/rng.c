// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2004 Makoto Matsumoto and Takuji Nishimura

// A C-program for MT19937-64 (2004/9/29 version).
// Coded by Takuji Nishimura and Makoto Matsumoto.
//
// This is a 64-bit version of Mersenne Twister pseudorandom number
// generator.
//
// Before using, initialize the state by using init_genrand64(seed)
// or init_by_array64(init_key, key_length).
//
// References:
// T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
//   ACM Transactions on Modeling and
//   Computer Simulation 10. (2000) 348--357.
// M. Matsumoto and T. Nishimura,
//   ``Mersenne Twister: a 623-dimensionally equidistributed
//     uniform pseudorandom number generator''
//   ACM Transactions on Modeling and
//   Computer Simulation 8. (Jan. 1998) 3--30.
//
// Any feedback is very welcome.
// http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
// email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)

// The code has been modified to store internal state in heap/stack
// allocated memory, rather than statically allocated memory, to allow
// multiple instances running in the same address space.
//
// Also, the functions in the module's public interface have
// been prefixed with "fuzz_rng_".

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "rng.h"

#define FUZZ_MT_PARAM_N 312
struct fuzz_rng {
	uint64_t mt[FUZZ_MT_PARAM_N]; // the array for the state vector
	int16_t  mti;
};

#define NN       FUZZ_MT_PARAM_N
#define MM       156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM       0xFFFFFFFF80000000ULL // Most significant 33 bits
#define LM       0x7FFFFFFFULL         // Least significant 31 bits

static uint64_t genrand64_int64(struct fuzz_rng* r);

// Heap-allocate a mersenne twister struct.
struct fuzz_rng*
fuzz_rng_init(uint64_t seed)
{
	struct fuzz_rng* mt = malloc(sizeof(struct fuzz_rng));
	if (mt == NULL) {
		return NULL;
	}
	fuzz_rng_reset(mt, seed);
	return mt;
}

// Free a heap-allocated mersenne twister struct.
void
fuzz_rng_free(struct fuzz_rng* mt)
{
	free(mt);
}

// initializes mt[NN] with a seed
void
fuzz_rng_reset(struct fuzz_rng* mt, uint64_t seed)
{
	mt->mt[0]    = seed;
	uint16_t mti = 0;
	for (mti = 1; mti < NN; mti++) {
		uint64_t tmp = (mt->mt[mti - 1] ^ (mt->mt[mti - 1] >> 62));
		mt->mt[mti]  = 6364136223846793005ULL * tmp + mti;
	}

	mt->mti = mti;
}

// Get a 64-bit random number.
uint64_t
fuzz_rng_random(struct fuzz_rng* mt)
{
	return genrand64_int64(mt);
}

// Generate a random number on [0,1]-real-interval.
double
fuzz_rng_uint64_to_double(uint64_t x)
{
	return (x >> 11) * (1.0 / 9007199254740991.0);
}

// generates a random number on [0, 2^64-1]-interval
static uint64_t
genrand64_int64(struct fuzz_rng* r)
{
	int             i;
	uint64_t        x;
	static uint64_t mag01[2] = {0ULL, MATRIX_A};

	if (r->mti >= NN) { // generate NN words at one time

		// if init has not been called,
		// a default initial seed is used
		if (r->mti == NN + 1)
			fuzz_rng_reset(r, 5489ULL);

		for (i = 0; i < NN - MM; i++) {
			x        = (r->mt[i] & UM) | (r->mt[i + 1] & LM);
			r->mt[i] = r->mt[i + MM] ^ (x >> 1) ^
				   mag01[(int)(x & 1ULL)];
		}
		for (; i < NN - 1; i++) {
			x        = (r->mt[i] & UM) | (r->mt[i + 1] & LM);
			r->mt[i] = r->mt[i + (MM - NN)] ^ (x >> 1) ^
				   mag01[(int)(x & 1ULL)];
		}
		x             = (r->mt[NN - 1] & UM) | (r->mt[0] & LM);
		r->mt[NN - 1] = r->mt[MM - 1] ^ (x >> 1) ^
				mag01[(int)(x & 1ULL)];

		r->mti = 0;
	}

	x = r->mt[r->mti++];

	x ^= (x >> 29) & 0x5555555555555555ULL;
	x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
	x ^= (x << 37) & 0xFFF7EEE000000000ULL;
	x ^= (x >> 43);

	return x;
}
