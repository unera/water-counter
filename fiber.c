#include <fiber.h>
#include <list.h>
#include <stdint.h>
#include <avr/io.h>

typedef uint16_t		code_t;

struct fiber {
	struct list_head	list;
	fiber_cb		cb;
	code_t			sp;		// stack pointer
};

struct fiber *current;

LIST_HEAD(ready);
LIST_HEAD(sch);

static void
fiber_run(void)
{
	current->cb();
	// schedule the fiber
}

struct fiber *
fiber_create(fiber_cb cb, void *stack, size_t stack_size)
{
	struct fiber *c = (struct fiber *)stack;
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

	next = list_entry(&ready.next, struct fiber, list);
	if (current == next)
		return;

	prev = current;

	list_del(&prev->list);
	list_add_tail(&prev->list, &ready);
	current = next;


	/* switch stack! */
	prev->sp = SP;
	SP = next->sp;
}

void
fiber_schedule(void)
{
	static struct fiber *prev, *next;

	if (!current)
		return;
	for (;;) { // current fiber is last
		next = list_entry(&ready.next, struct fiber, list);
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
	list_del(&f->list);
	list_add_tail(&f->list, &ready);
}
