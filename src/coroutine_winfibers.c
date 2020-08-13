/*
 * SpiceGtk coroutine with Windows fibers
 *
 * Copyright (C) 2011  Marc-André Lureau <marcandre.lureau@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"
#include <stdio.h>
#include <glib.h>

#include "coroutine.h"

static struct coroutine leader = { 0, };
static struct coroutine *current = &leader;

int coroutine_release(struct coroutine *co)
{
	DeleteFiber(co->fiber);
	return 0;
}

static void WINAPI coroutine_trampoline(LPVOID lpParameter)
{
	struct coroutine *co = (struct coroutine *)lpParameter;
	struct coroutine *caller = co->caller;

	co->data = co->entry(co->data);

	co->caller = NULL;

	// and switch back to caller
	co->ret = 1;
	SwitchToFiber(caller->fiber);
}

void coroutine_init(struct coroutine *co)
{
	if (leader.fiber == NULL) {
		leader.fiber = ConvertThreadToFiber(&leader);
		if (leader.fiber == NULL)
			g_error("ConvertThreadToFiber() failed");
	}

	co->exited = 0;
	co->fiber = CreateFiber(0, &coroutine_trampoline, co);
	if (co->fiber == NULL)
		g_error("CreateFiber() failed");

	co->ret = 0;
}

struct coroutine *coroutine_self(void)
{
	return current;
}

static void *coroutine_swap(struct coroutine *from, struct coroutine *to, void *arg)
{
	to->data = arg;
	current = to;
	SwitchToFiber(to->fiber);
	if (to->ret == 0)
		return from->data;
	else if (to->ret == 1) {
		coroutine_release(to);
		current = from;
		to->exited = 1;
		return to->data;
	}

	return NULL;
}

void *coroutine_yieldto(struct coroutine *to, void *arg)
{
	g_return_val_if_fail(!to->caller, NULL);
	g_return_val_if_fail(!to->exited, NULL);

	to->caller = coroutine_self();
	return coroutine_swap(coroutine_self(), to, arg);
}

void *coroutine_yield(void *arg)
{
	struct coroutine *to = coroutine_self()->caller;
	if (!to) {
		fprintf(stderr, "Co-routine is yielding to no one\n");
		abort();
	}
	coroutine_self()->caller = NULL;
	return coroutine_swap(coroutine_self(), to, arg);
}

gboolean coroutine_is_main(struct coroutine *co)
{
	return (co == &leader);
}
/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
