#include <fiber.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>

/* part from linux-kernel list.h adapted for AVR */
struct list_head {
	struct list_head *next, *prev;
};
#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

/*
 * Insert a newl entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void
__list_add(struct list_head *newl,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = newl;
	newl->next = next;
	newl->prev = prev;
	prev->next = newl;
}

/**
 * list_add - add a newl entry
 * @newl: newl entry to be added
 * @head: list head to add it after
 *
 * Insert a newl entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void
list_add(struct list_head *newl, struct list_head *head)
{
	__list_add(newl, head, head->next);
}

/**
 * list_add_tail - add a newl entry
 * @newl: newl entry to be added
 * @head: list head to add it before
 *
 * Insert a newl entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void
list_add_tail(struct list_head *newl, struct list_head *head)
{
	__list_add(newl, head->prev, head);
}

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
		pos = pos->next)

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(uint16_t)(&((type *)0)->member)))

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void
__list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this,
 * the entry is in an undefined state.
 */
static inline void
list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = (struct list_head *) 0;
	entry->prev = (struct list_head *) 0;
}

/*************************************************/


enum fiber_state {
	READY		= 'r',
	STARTING	= 'R',
	SCHEDULE	= 's',
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
_fiber_run(void)
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
	c->sp = (code_t)(((uint8_t *)stack) + stack_size - 1);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_add_tail(&c->list, &ready);
	}
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
	struct fiber *prev;
	prev = current;

	uint8_t real_sreg;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_del(&prev->list);
		if (list)
			list_add_tail(&prev->list, list);
		current = next;

		real_sreg	= sreg_save;

		/* switch stack! */
		prev->sp = SP;
		SP = next->sp;

		if (current->state == STARTING) {
			// hack!: we hope ATOMIC_RESTORESTATE is unchanged
			// in the future
			SREG = real_sreg;
			// don't return here
			_fiber_run();
		}
	}

	SREG = real_sreg;
	current->state = READY;
}

static struct fiber *
_fiber_fetch(uint8_t no, struct list_head *head)
{
	struct list_head *pos;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		list_for_each(pos, head) {
			if (no--)
				continue;
			return list_entry(pos, struct fiber, list);
		}
	}
	return NULL;
}


void
fiber_cede(void)
{
	if (!current)		// not init yet
		return;

	struct fiber *next;
	while (!(next = _fiber_fetch(1, &ready)))
		return;

	return _fiber_switch(next, &ready);
}

static void
_fiber_schedule(enum fiber_state new_state, struct list_head *list)
{
	if (!current)
		return;
	current->state = new_state;
	struct fiber *next;
	while (!(next = _fiber_fetch(1, &ready)));
	_fiber_switch(next, list);
}

void
fiber_schedule(void)
{
	return _fiber_schedule(SCHEDULE, &sch);
}

void
fiber_wakeup(struct fiber *f)
{
	if (f->state != SCHEDULE)
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
		case DEAD:
		case CANCELLED:
			return;
		case READY:
			break;
		case SCHEDULE:
		case STARTING:
			f->state = CANCELLED;
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				list_del(&f->list);
			}
			return;
	}
	if (f != current) {
		f->state = CANCELLED;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			list_del(&f->list);
		}
		return;
	}
	// self cancel
	return _fiber_schedule(CANCELLED, NULL);
}

unsigned char
fiber_status(const struct fiber *f)
{
	return f->state;
}
