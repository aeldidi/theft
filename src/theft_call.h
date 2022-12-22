// SPDX-License-Identifier: ISC
// SPDX-FileCopyrightText: 2014-19 Scott Vokes <vokes.s@gmail.com>
#ifndef THEFT_CALL_H
#define THEFT_CALL_H

#include <stdbool.h>

struct theft;

/* Actually call the property function referenced in INFO,
 * with the arguments in ARGS. */
int theft_call(struct theft* t, void** args);

/* Check if this combination of argument instances has been called. */
bool theft_call_check_called(struct theft* t);

/* Mark the tuple of argument instances as called in the bloom filter. */
void theft_call_mark_called(struct theft* t);

#endif
