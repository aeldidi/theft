// SPDX-License-Identifier: ISC
// SPDX-FileCopyrightText: 2014-19 Scott Vokes <vokes.s@gmail.com>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "fuzz.h"
#include "test_fuzz_autoshrink_ll.h"

static void ll_free(void* instance, void* env);
static void ll_print(FILE* f, const void* instance, void* env);

// Generate a linked list of uint8_t values. Before each
// link, get 3 random bits -- if 0, then end the list.
static int
ll_alloc(struct fuzz* t, void* env, void** instance)
{
	(void)env;
	struct ll* res  = NULL;
	struct ll* prev = NULL;

	// Each link has a 1 in 8 chance of end-of-list
	while (fuzz_random_bits(t, 3) != 0x00) {
		struct ll* link = calloc(1, sizeof(struct ll));
		if (link == NULL) {
			ll_free(res, NULL);
			return FUZZ_RESULT_ERROR;
		}

		link->tag   = 'L';
		link->value = (uint8_t)fuzz_random_bits(t, 8);

		if (res == NULL) {
			res  = link;
			prev = link;
		} else {
			prev->next = link;
			prev       = link;
		}
	}

	*instance = res;
	if (0) {
		printf("    ALLOC: ");
		ll_print(stdout, res, NULL);
		printf("\n");
	}
	return FUZZ_RESULT_OK;
}

static void
ll_free(void* instance, void* env)
{
	(void)env;
	struct ll* cur = (struct ll*)instance;

	while (cur) {
		assert(cur->tag == 'L');
		struct ll* next = cur->next;
		free(cur);
		cur = next;
	}
}

static void
ll_print(FILE* f, const void* instance, void* env)
{
	(void)env;
	const struct ll* cur = (struct ll*)instance;

	fprintf(f, "[");
	while (cur) {
		assert(cur->tag == 'L');
		const struct ll* next = cur->next;
		fprintf(f, "%u ", cur->value);
		cur = next;
	}
	fprintf(f, "]");
}

struct fuzz_type_info ll_info = {
		.alloc = ll_alloc,
		.free  = ll_free,
		.print = ll_print,
		.autoshrink_config =
				{
						.enable = true,
						.print_mode = FUZZ_AUTOSHRINK_PRINT_ALL,
				},
};
