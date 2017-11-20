#include <fiber.h>
#include <list.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <avr/interrupt.h>

enum fiber_state {
	READY		= 'r',
	STARTING	= 'R',
	SLEEP		= 's',
	DEAD		= 'd',
	CANCELLED	= 'c'
};
typedef uint16_t		code_t;
struct fiber {
	enum fiber_state	state;
	fiber_cb		cb;		// fiber function
	code_t			sp;		// stack pointer

	struct list_head	list;
};

static struct fiber *current;
static LIST_HEAD(ready);
static LIST_HEAD(sch);

static void
fiber_run(void)
{
	current->state = READY;
	for(;;) {
		current->cb();
		current->state = DEAD;
		fiber_schedule();
	}
}

struct fiber *
fiber_create(fiber_cb cb, void *stack, size_t stack_size)
{
	struct fiber *c = (struct fiber *)stack;
	c->state = STARTING;
	c->cb = cb;
	c->sp = (code_t)(((char *)stack) + stack_size - 1);
	list_add_tail(&c->list, &ready);
	code_t start = (code_t)&fiber_run;

	*((char *)c->sp--) = start >> 8;
	*((char *)c->sp--) = start & 0xff;
	return c;
}

// return current fiber
struct fiber *
fiber_current(void)
{
	return current;
}


void
_fiber_switch(struct fiber *next, struct list_head *list)
{
	static struct fiber *prev;
	prev = current;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_del(&prev->list);
		list_add_tail(&prev->list, list);
		current = next;

		/* switch stack! */
		prev->sp = SP;
		SP = next->sp;
	}

	if (current->state == STARTING) {
		fiber_run();
	}
	current->state = READY;
}

void
fiber_cede(void)
{
	static struct fiber *next;

	if (!current)		// not init yet
		return;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		next = list_entry(&ready.next, struct fiber, list);
	}

	if (current == next)
		return;

	_fiber_switch(next, &ready);
}

void
fiber_schedule(void)
{
	static struct fiber *next;

	if (!current)
		return;
	current->state = SLEEP;
	for (;;) { // current fiber is last
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			next = list_entry(&ready.next, struct fiber, list);
		}
		if (current != next)
			break;
	}

	_fiber_switch(next, &sch);
}

void
fiber_wakeup(struct fiber *f)
{
	if (f->state != SLEEP)
		return;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		f->state = READY;
		list_del(&f->list);
		list_add_tail(&f->list, &ready);
	}
}

void
fibers_init(void)
{
	static struct fiber _main;
	if (current)			// already init done
		return;
	_main.state = READY;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_add_tail(&_main.list, &ready);
		current = &_main;
	}
}

void
fiber_cancel(struct fiber *f)
{
	switch(f->state) {
		case CANCELLED:
		case READY:		// something wrong!
			return;
		case SLEEP:
		case DEAD:
		case STARTING:
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				list_del(&f->list);
			}
			break;
	}
}
