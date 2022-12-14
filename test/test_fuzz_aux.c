// SPDX-License-Identifier: ISC
// SPDX-FileCopyrightText: 2014-19 Scott Vokes <vokes.s@gmail.com>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "fuzz.h"
#include "greatest.h"
#include "types_internal.h"

struct a_squared_env {
	struct fuzz_print_trial_result_env print_env;
	bool                               found;
};

// Property: a^2 <= 12345
static int
prop_a_squared_lte_fixed(struct fuzz* t, void* arg1)
{
	void* arg = (void*)arg1;
	(void)t;
	int8_t       a  = *(int8_t*)arg;
	uint16_t     b  = 12345;
	const size_t aa = a * a;

	return (aa <= b) ? FUZZ_RESULT_OK : FUZZ_RESULT_FAIL;
}

static int
fixed_expected_failure_trial_post(
		const struct fuzz_post_trial_info* info, void* penv)
{
	struct a_squared_env* env = (struct a_squared_env*)penv;
	int8_t                a   = *(int8_t*)info->args[0];

	if (greatest_get_verbosity() > 0) {
		printf("### trial_post: res %s for [arg: %d (%p)]\n\n",
				fuzz_result_str(info->result), a,
				(void*)info->args[0]);
	}

	// Set found if the min failure is found.
	if (info->result == FUZZ_RESULT_FAIL) {
		int8_t a = *(int8_t*)info->args[0];
		//printf("FAILURE: %d\n", a);
		if (a == 112 || a == -112) { // min failure: 112 * 112 > 12345
			env->found = true;
		}
	}

	return FUZZ_HOOK_RUN_CONTINUE;
}

static int
fixed_log_shrink_trial_post(
		const struct fuzz_post_shrink_trial_info* info, void* env)
{
	(void)env;

	int8_t a = *(int8_t*)info->args[0];

	if (greatest_get_verbosity() > 0) {
		printf("### shrink_trial_post: res %s for arg %u [arg: %d "
		       "(%p)]\n\n",
				fuzz_result_str(info->result), info->arg_index,
				a, (void*)info->args[0]);
	}
	return FUZZ_HOOK_RUN_CONTINUE;
}

TEST
a_squared_lte_fixed(void)
{
	uint64_t seed = fuzz_seed_of_time();

	struct a_squared_env env;
	memset(&env, 0x00, sizeof(env));

	struct fuzz_run_config cfg = {
			.name  = __func__,
			.prop1 = prop_a_squared_lte_fixed,
			.type_info =
					{
							fuzz_get_builtin_type_info(
									FUZZ_BUILTIN_int8_t),
					},
			.bloom_bits = 20,
			.seed       = seed,
			.trials     = 500,
			.hooks =
					{
							.post_trial = fixed_expected_failure_trial_post,
							.post_shrink_trial =
									fixed_log_shrink_trial_post,
							.env = &env,
					},
	};

	ASSERT_EQ_FMTm("should find counter-examples", FUZZ_RESULT_FAIL,
			fuzz_run(&cfg), "%d");
	ASSERTm("Should shrink to a minimal case", env.found);
	PASS();
}

static int
prop_a_squared_lt_b(struct fuzz* t, void* arg1, void* arg2)
{
	(void)t;
	int8_t       a  = *(int8_t*)arg1;
	uint16_t     b  = *(uint16_t*)arg2;
	const size_t aa = a * a;

	if (0) {
		fprintf(stdout, "\n$$ checking (%d^2) < %u ? %d (a^2 = %zd)\n",
				a, b, aa < b, aa);
	}

	return (aa <= b) ? FUZZ_RESULT_OK : FUZZ_RESULT_FAIL;
}

static int
expected_failure_trial_post(
		const struct fuzz_post_trial_info* info, void* penv)
{
	struct a_squared_env* env = (struct a_squared_env*)penv;
	int8_t                a   = *(int8_t*)info->args[0];
	uint16_t              b   = *(uint16_t*)info->args[1];

	if (greatest_get_verbosity() > 0) {
		printf("### trial_post: res %s for [args: %d (%p), %u "
		       "(%p)]\n\n",
				fuzz_result_str(info->result), a,
				(void*)info->args[0], b, (void*)info->args[1]);
	}

	if (info->result == FUZZ_RESULT_FAIL) {
		int8_t   a = *(int8_t*)info->args[0];
		uint16_t b = *(uint16_t*)info->args[1];
		if (a == 1 && b == 0) {
			env->found = true;
		}
	}

	return FUZZ_HOOK_RUN_CONTINUE;
}

static int
log_shrink_trial_post(
		const struct fuzz_post_shrink_trial_info* info, void* env)
{
	(void)env;

	int8_t   a = *(int8_t*)info->args[0];
	uint16_t b = *(uint16_t*)info->args[1];

	if (greatest_get_verbosity() > 0) {
		printf("### shrink_trial_post: res %s for arg %u [args: %d "
		       "(%p), %u (%p)]\n\n",
				fuzz_result_str(info->result), info->arg_index,
				a, (void*)info->args[0], b,
				(void*)info->args[1]);
	}
	return FUZZ_HOOK_RUN_CONTINUE;
}

TEST
a_squared_lt_b(void)
{
	uint64_t seed = fuzz_seed_of_time();

	struct a_squared_env env;
	memset(&env, 0x00, sizeof(env));

	struct fuzz_run_config cfg = {
			.name  = __func__,
			.prop2 = prop_a_squared_lt_b,
			.type_info =
					{
							fuzz_get_builtin_type_info(
									FUZZ_BUILTIN_int8_t),
							fuzz_get_builtin_type_info(
									FUZZ_BUILTIN_uint16_t),
					},
			.bloom_bits = 20,
			.seed       = seed,
			.trials     = 500,
			.hooks =
					{
							.post_trial = expected_failure_trial_post,
							.post_shrink_trial =
									log_shrink_trial_post,
							.env = &env,
					},
	};

	ASSERT_EQ_FMTm("should find counter-examples", FUZZ_RESULT_FAIL,
			fuzz_run(&cfg), "%d");
	ASSERTm("Should shrink to a minimal case", env.found);
	PASS();
}

static int
prop_pass(struct fuzz* t, void* arg1)
{
	uint64_t* v = (uint64_t*)arg1;
	(void)t;
	(void)v;
	return FUZZ_RESULT_OK;
}

TEST
pass_autoscaling(void)
{
	struct fuzz_run_config cfg = {
			.name      = __func__,
			.prop1     = prop_pass,
			.type_info = {fuzz_get_builtin_type_info(
					FUZZ_BUILTIN_uint64_t)},
			.trials    = 1000000,
	};

	int res = fuzz_run(&cfg);

	// This test needs to be checked by visual inspection -- it should
	// look something like this:
	//
	// == PROP 'pass_autoscaling': 1000000 trials, seed 0x00a600d64b175eed
	// .........d.............d.d...d.............d...........................
	// ................................d..(PASS x 100).dddd(DUP x 10)d.dd.d.dd
	// .d.d.d(DUP x 100)d.......d.......d........d.......d........d......d....
	// ....d......d.......(DUP x 1000)d............................
	// (PASS x 10000).d.dd.dd.dd.d(DUP x 10000)d..d..d.dd.ddd.d(DUP x 100000)d
	// .ddddddd
	// == PASS 'pass_autoscaling': pass 136572, fail 0, skip 0, dup 863428
	//
	// where the acceptance criteria is that it prints `PASS x 10000`
	// rather than million different '.' and 'd' characters along the
	// way. (If it fails, it should completely overwhelm the test output
	// in an obvious way.)
	(void)res;

	PASS();
}

static int
prop_check_and_update_magic(struct fuzz* t, void* arg1)
{
	uint64_t* unused = (uint64_t*)arg1;
	(void)unused;

	uint64_t* magic = (uint64_t*)fuzz_hook_get_env(t);
	if (magic == NULL || *magic != 0xABED) {
		return FUZZ_RESULT_FAIL;
	}

	*magic = 0xfa1afel; // update the magic value
	return FUZZ_RESULT_OK;
}

TEST
get_hook_env(void)
{
	uint64_t magic = 0xABED;

	struct fuzz_run_config cfg = {
			.name      = __func__,
			.prop1     = prop_check_and_update_magic,
			.type_info = {fuzz_get_builtin_type_info(
					FUZZ_BUILTIN_uint64_t)},
			.trials    = 1,
			.hooks =
					{
							.env = (void*)&magic,
					},
	};
	int res = fuzz_run(&cfg);

	ASSERT_EQ_FMTm("should get pointer to the magic value", FUZZ_RESULT_OK,
			res, "%d");
	ASSERT_EQ_FMTm("should update the magic value", (uint64_t)0xfa1afel,
			magic, "%" PRIx64);

	PASS();
}

struct gap_env {
	uint8_t  tag;
	uint64_t limit;
	bool     used;
};

static int
gap_alloc(struct fuzz* t, void* info_env, void** output)
{
	(void)info_env;
	struct gap_env* env = (struct gap_env*)fuzz_hook_get_env(t);
	if (env->tag != 'G') {
		return FUZZ_RESULT_ERROR;
	}
	uint64_t* res = malloc(sizeof(*res));
	if (res == NULL) {
		return FUZZ_RESULT_ERROR;
	}
	*res      = fuzz_random_bits(t, 64) % env->limit;
	env->used = true;

	*output = res;
	return FUZZ_RESULT_OK;
}

static void
gap_print(FILE* f, const void* instance, void* env)
{
	(void)env;
	uint64_t* v = (uint64_t*)instance;
	fprintf(f, "%" PRIu64, *v);
}

struct fuzz_type_info gap_info = {
		.alloc = gap_alloc,
		.free  = fuzz_generic_free_cb,
		.print = gap_print,
};

// Generate a uint64_t, but limit it to < 1000, to
// verify that the hook_env is passed along correctly.
TEST
gen_and_print(void)
{
	uint64_t seed = fuzz_seed_of_time();

	struct gap_env env = {
			.tag   = 'G',
			.limit = 1000,
	};

	int res = fuzz_generate(stdout, seed, &gap_info, &env);

	ASSERT_EQ_FMT(FUZZ_RESULT_OK, res, "%d");
	ASSERT(env.used);

	PASS();
}

SUITE(aux)
{
	// builtins
	RUN_TEST(a_squared_lte_fixed);
	RUN_TEST(a_squared_lt_b);

	// Tests for other misc. aux stuff
	RUN_TEST(pass_autoscaling);
	RUN_TEST(get_hook_env);
	RUN_TEST(gen_and_print);
}
