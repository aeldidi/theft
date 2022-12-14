// SPDX-License-Identifier: ISC
// SPDX-FileCopyrightText: 2014-19 Scott Vokes <vokes.s@gmail.com>
#include <assert.h>

#include "autoshrink.h"
#include "call.h"
#include "fuzz.h"
#include "shrink.h"
#include "trial.h"
#include "types_internal.h"

enum shrink_res {
	SHRINK_OK,       // simplified argument further
	SHRINK_DEAD_END, // at local minima
	SHRINK_ERROR,    // hard error during shrinking
	SHRINK_HALT,     // don't shrink any further
};

static enum shrink_res attempt_to_shrink_arg(struct fuzz* t, uint8_t arg_i);

static int shrink_pre_hook(
		struct fuzz* t, uint8_t arg_index, void* arg, uint32_t tactic);

static int shrink_post_hook(struct fuzz* t, uint8_t arg_index, void* arg,
		uint32_t tactic, int sres);

static int shrink_trial_post_hook(struct fuzz* t, uint8_t arg_index,
		void** args, uint32_t last_tactic, int result);

#define LOG_SHRINK 0

// Attempt to simplify all arguments, breadth first. Continue as long as
// progress is made, i.e., until a local minimum is reached.
bool
fuzz_shrink(struct fuzz* t)
{
	bool progress = false;
	assert(t->prop.arity > 0);

	do {
		progress = false;
		// Greedily attempt to simplify each argument as much as
		// possible before switching to the next.
		for (uint8_t arg_i = 0; arg_i < t->prop.arity; arg_i++) {
			struct fuzz_type_info* ti = t->prop.type_info[arg_i];
		greedy_continue:
			if (ti->shrink || ti->autoshrink_config.enable) {
				// attempt to simplify this argument by one
				// step
				enum shrink_res rres = attempt_to_shrink_arg(
						t, arg_i);

				switch (rres) {
				case SHRINK_OK:
					LOG(3 - LOG_SHRINK,
							"%s %u: progress\n",
							__func__, arg_i);
					progress = true;
					goto greedy_continue; // keep trying to
							      // shrink same
							      // argument
				case SHRINK_HALT:
					LOG(3 - LOG_SHRINK, "%s %u: HALT\n",
							__func__, arg_i);
					return true;
				case SHRINK_DEAD_END:
					LOG(3 - LOG_SHRINK,
							"%s %u: DEAD END\n",
							__func__, arg_i);
					continue; // try next argument, if any
				default:
				case SHRINK_ERROR:
					LOG(1 - LOG_SHRINK, "%s %u: ERROR\n",
							__func__, arg_i);
					return false;
				}
			}
		}
	} while (progress);
	return true;
}

// Simplify an argument by trying all of its simplification tactics, in
// order, and checking whether the property still fails. If it passes,
// then revert the simplification and try another tactic.
//
// If the bloom filter is being used (i.e., if all arguments have hash
// callbacks defined), then use it to skip over areas of the state
// space that have probably already been tried.
static enum shrink_res
attempt_to_shrink_arg(struct fuzz* t, uint8_t arg_i)
{
	struct fuzz_type_info* ti             = t->prop.type_info[arg_i];
	const bool             use_autoshrink = ti->autoshrink_config.enable;

	for (uint32_t tactic = 0; tactic < FUZZ_MAX_TACTICS; tactic++) {
		LOG(2 - LOG_SHRINK, "SHRINKING arg %u, tactic %u\n", arg_i,
				tactic);
		void* current   = t->trial.args[arg_i].instance;
		void* candidate = NULL;

		int shrink_pre_res;
		shrink_pre_res = shrink_pre_hook(t, arg_i, current, tactic);
		if (shrink_pre_res == FUZZ_HOOK_RUN_HALT) {
			return SHRINK_HALT;
		} else if (shrink_pre_res != FUZZ_HOOK_RUN_CONTINUE) {
			return SHRINK_ERROR;
		}

		struct autoshrink_env*      as_env             = NULL;
		struct autoshrink_bit_pool* current_bit_pool   = NULL;
		struct autoshrink_bit_pool* candidate_bit_pool = NULL;
		if (use_autoshrink) {
			as_env = t->trial.args[arg_i].u.as.env;
			assert(as_env);
			current_bit_pool = t->trial.args[arg_i]
							   .u.as.env->bit_pool;
		}

		int sres = (use_autoshrink ? fuzz_autoshrink_shrink(t, as_env,
							     tactic,
							     &candidate,
							     &candidate_bit_pool)
					   : ti->shrink(t, current, tactic,
							     ti->env,
							     &candidate));

		LOG(3 - LOG_SHRINK, "%s: tactic %u -> res %d\n", __func__,
				tactic, sres);

		t->trial.shrink_count++;

		int shrink_post_res;
		shrink_post_res = shrink_post_hook(t, arg_i,
				sres == FUZZ_SHRINK_OK ? candidate : current,
				tactic, sres);
		if (shrink_post_res != FUZZ_HOOK_RUN_CONTINUE) {
			if (ti->free) {
				ti->free(candidate, ti->env);
			}
			if (candidate_bit_pool) {
				fuzz_autoshrink_free_bit_pool(
						t, candidate_bit_pool);
			}
			return SHRINK_ERROR;
		}

		switch (sres) {
		case FUZZ_SHRINK_OK:
			break;
		case FUZZ_SHRINK_DEAD_END:
			continue; // try next tactic
		case FUZZ_SHRINK_NO_MORE_TACTICS:
			return SHRINK_DEAD_END;
		case FUZZ_SHRINK_ERROR:
		default:
			return SHRINK_ERROR;
		}

		t->trial.args[arg_i].instance = candidate;
		if (use_autoshrink) {
			as_env->bit_pool = candidate_bit_pool;
		}

		if (t->bloom) {
			if (fuzz_call_check_called(t)) {
				LOG(3 - LOG_SHRINK,
						"%s: already called, "
						"skipping\n",
						__func__);
				if (ti->free) {
					ti->free(candidate, ti->env);
				}
				if (use_autoshrink) {
					as_env->bit_pool = current_bit_pool;
					fuzz_autoshrink_free_bit_pool(
							t, candidate_bit_pool);
				}
				t->trial.args[arg_i].instance = current;
				continue;
			} else {
				fuzz_call_mark_called(t);
			}
		}

		int  res;
		bool repeated = false;
		for (;;) {
			void* args[FUZZ_MAX_ARITY];
			fuzz_trial_get_args(t, args);

			res = fuzz_call(t, args);
			LOG(3 - LOG_SHRINK, "%s: call -> res %d\n", __func__,
					res);

			if (!repeated) {
				if (res == FUZZ_RESULT_FAIL) {
					t->trial.successful_shrinks++;
					fuzz_autoshrink_update_model(
							t, arg_i, res, 3);
				} else {
					t->trial.failed_shrinks++;
				}
			}

			int stpres;
			stpres = shrink_trial_post_hook(
					t, arg_i, args, tactic, res);
			if (stpres == FUZZ_HOOK_RUN_REPEAT ||
					(stpres == FUZZ_HOOK_RUN_REPEAT_ONCE &&
							!repeated)) {
				repeated = true;
				continue; // loop and run again
			} else if (stpres == FUZZ_HOOK_RUN_REPEAT_ONCE &&
					repeated) {
				break;
			} else if (stpres == FUZZ_HOOK_RUN_CONTINUE) {
				break;
			} else {
				if (ti->free) {
					ti->free(current, ti->env);
				}
				if (use_autoshrink && current_bit_pool) {
					fuzz_autoshrink_free_bit_pool(
							t, current_bit_pool);
				}
				return SHRINK_ERROR;
			}
		}

		fuzz_autoshrink_update_model(t, arg_i, res, 8);

		switch (res) {
		case FUZZ_RESULT_OK:
		case FUZZ_RESULT_SKIP:
			LOG(2 - LOG_SHRINK,
					"PASS or SKIP: REVERTING %u: "
					"candidate %p (pool %p), back to %p "
					"(pool %p)\n",
					arg_i, (void*)candidate,
					(void*)candidate_bit_pool,
					(void*)current,
					(void*)current_bit_pool);
			t->trial.args[arg_i].instance = current;
			if (use_autoshrink) {
				fuzz_autoshrink_free_bit_pool(
						t, candidate_bit_pool);
				t->trial.args[arg_i].u.as.env->bit_pool =
						current_bit_pool;
			}
			if (ti->free) {
				ti->free(candidate, ti->env);
			}
			break;
		case FUZZ_RESULT_FAIL:
			LOG(2 - LOG_SHRINK,
					"FAIL: COMMITTING %u: was %p (pool "
					"%p), now %p (pool %p)\n",
					arg_i, (void*)current,
					(void*)current_bit_pool,
					(void*)candidate,
					(void*)candidate_bit_pool);
			if (use_autoshrink) {
				assert(t->trial.args[arg_i].u.as.env
								->bit_pool ==
						candidate_bit_pool);
				fuzz_autoshrink_free_bit_pool(
						t, current_bit_pool);
			}
			assert(t->trial.args[arg_i].instance == candidate);
			if (ti->free) {
				ti->free(current, ti->env);
			}
			return SHRINK_OK;
		default:
		case FUZZ_RESULT_ERROR:
			if (ti->free) {
				ti->free(current, ti->env);
			}
			if (use_autoshrink) {
				fuzz_autoshrink_free_bit_pool(
						t, current_bit_pool);
			}
			return SHRINK_ERROR;
		}
	}
	(void)t;
	return SHRINK_DEAD_END;
}

static int
shrink_pre_hook(struct fuzz* t, uint8_t arg_index, void* arg, uint32_t tactic)
{
	if (t->hooks.shrink_pre != NULL) {
		struct fuzz_pre_shrink_info hook_info = {
				.prop_name    = t->prop.name,
				.total_trials = t->prop.trial_count,
				.failures     = t->counters.fail,
				.run_seed     = t->seeds.run_seed,
				.trial_id     = t->trial.trial,
				.trial_seed   = t->trial.seed,
				.arity        = t->prop.arity,
				.shrink_count = t->trial.shrink_count,
				.successful_shrinks =
						t->trial.successful_shrinks,
				.failed_shrinks = t->trial.failed_shrinks,
				.arg_index      = arg_index,
				.arg            = arg,
				.tactic         = tactic,
		};
		return t->hooks.shrink_pre(&hook_info, t->hooks.env);
	} else {
		return FUZZ_HOOK_RUN_CONTINUE;
	}
}

static int
shrink_post_hook(struct fuzz* t, uint8_t arg_index, void* arg, uint32_t tactic,
		int sres)
{
	if (t->hooks.shrink_post != NULL) {
		enum fuzz_post_shrink_state state;
		switch (sres) {
		case FUZZ_SHRINK_OK:
			state = FUZZ_SHRINK_POST_SHRUNK;
			break;
		case FUZZ_SHRINK_NO_MORE_TACTICS:
			state = FUZZ_SHRINK_POST_DONE_SHRINKING;
			break;
		case FUZZ_SHRINK_DEAD_END:
			state = FUZZ_SHRINK_POST_SHRINK_FAILED;
			break;
		default:
			assert(false);
			return FUZZ_HOOK_RUN_ERROR;
		}

		struct fuzz_post_shrink_info hook_info = {
				.prop_name    = t->prop.name,
				.total_trials = t->prop.trial_count,
				.run_seed     = t->seeds.run_seed,
				.trial_id     = t->trial.trial,
				.trial_seed   = t->trial.seed,
				.arity        = t->prop.arity,
				.shrink_count = t->trial.shrink_count,
				.successful_shrinks =
						t->trial.successful_shrinks,
				.failed_shrinks = t->trial.failed_shrinks,
				.arg_index      = arg_index,
				.arg            = arg,
				.tactic         = tactic,
				.state          = state,
		};
		return t->hooks.shrink_post(&hook_info, t->hooks.env);
	} else {
		return FUZZ_HOOK_RUN_CONTINUE;
	}
}

static int
shrink_trial_post_hook(struct fuzz* t, uint8_t arg_index, void** args,
		uint32_t last_tactic, int result)
{
	if (t->hooks.shrink_trial_post != NULL) {
		struct fuzz_post_shrink_trial_info hook_info = {
				.prop_name    = t->prop.name,
				.total_trials = t->prop.trial_count,
				.failures     = t->counters.fail,
				.run_seed     = t->seeds.run_seed,
				.trial_id     = t->trial.trial,
				.trial_seed   = t->trial.seed,
				.arity        = t->prop.arity,
				.shrink_count = t->trial.shrink_count,
				.successful_shrinks =
						t->trial.successful_shrinks,
				.failed_shrinks = t->trial.failed_shrinks,
				.arg_index      = arg_index,
				.args           = args,
				.tactic         = last_tactic,
				.result         = result,
		};
		return t->hooks.shrink_trial_post(&hook_info, t->hooks.env);
	} else {
		return FUZZ_HOOK_RUN_CONTINUE;
	}
}
