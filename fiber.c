#include <fiber.h>
#include <list.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <avr/interrupt.h>

enum fiber_state { READY = 'r', SLEEP = 's', DEAD = 'd', CANCELLED = 'c' };
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
	c->state = READY;
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
fiber_cede(void)
{
	static struct fiber *prev, *next;

	// WARNING: DO NOT USE any local variables, only static!

	if (!current)		// not init yet
		return;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		next = list_entry(&ready.next, struct fiber, list);
	}

	if (current == next)
		return;

	prev = current;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_del(&prev->list);
		list_add_tail(&prev->list, &ready);
		current = next;

		/* switch stack! */
		prev->sp = SP + 2; // drop saved SREG
		SP = next->sp;
		// hack! We hope that macroses
		//  - ATOMIC_RESTORESTATE
		//  - ATOMIC_BLOCK
		// will not be changed in the future
		asm("push r10");
		asm("mov r10, sreg_save");		// hack!
/*                 asm("push sreg_save"); */
	}


}

void
fiber_schedule(void)
{
	static struct fiber *prev, *next;

	if (!current)
		return;
	for (;;) { // current fiber is last
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			next = list_entry(&ready.next, struct fiber, list);
		}
		if (current != next)
			break;
	}

	prev = current;

	list_del(&prev->list);
	list_add_tail(&prev->list, &sch);
	current = next;


	/* switch stack! */
	prev->sp = SP;
	SP = next->sp;
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
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				list_del(&f->list);
			}
			break;
	}
}
