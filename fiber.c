#include <fiber.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <string.h>

/* part from linux-kernel list.h adapted for AVR */
struct list_head { struct list_head *next, *prev; };

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)
#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

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

static inline void
list_add(struct list_head *newl, struct list_head *head)
{
	__list_add(newl, head, head->next);
}

static inline void
list_add_tail(struct list_head *newl, struct list_head *head)
{
	__list_add(newl, head->prev, head);
}

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
		pos = pos->next)

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(uint16_t)(&((type *)0)->member)))

static inline void
__list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

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
	SCHEDULED	= 's',
	DEAD		= 'd',
	CANCELLED	= 'c'
};
typedef uint16_t		code_t;
struct fiber {
	struct list_head	list;		// member
	enum fiber_state	state;
	fiber_cb		cb;		// fiber function
	code_t			sp;		// stack pointer

	struct list_head	join;
	uint8_t data[0];			// TODO: use the area in join
};

static struct fiber *current;
static LIST_HEAD(ready);
static LIST_HEAD(sch);
static LIST_HEAD(dead);

static void _fiber_run(void);
static struct fiber * _fiber_fetch(uint8_t no, struct list_head *head);
void _fiber_switch(struct fiber *next, struct list_head *list);
static void _fiber_schedule(enum fiber_state new_state, struct list_head *list);


struct fiber *
fiber_create(fiber_cb cb, void *stack, size_t stack_size)
{
	struct fiber *c = (struct fiber *)stack;
	c->state = STARTING;
	c->cb = cb;
	c->sp = (code_t)(((uint8_t *)stack) + stack_size - 1);
	INIT_LIST_HEAD(&c->join);
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
fiber_cede(void)
{
	if (!current)		// not init yet
		return;

	struct fiber *next;
	while (!(next = _fiber_fetch(1, &ready)))
		return;

	return _fiber_switch(next, &ready);
}

void
fiber_schedule(void)
{
	return _fiber_schedule(SCHEDULED, &sch);
}

void
fiber_wakeup(struct fiber *f)
{
	if (f->state != SCHEDULED)
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
	memset(&_main, 0, sizeof(_main));
	_main.state = READY;
	INIT_LIST_HEAD(&_main.join);
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
		case SCHEDULED:
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

const void *
fiber_join(struct fiber *f)
{
	if (f == current)
		return NULL;
	switch(f->state) {
		case STARTING:
		case READY:
		case SCHEDULED:
			break;
		default:
			return NULL;
	}

	_fiber_schedule(SCHEDULED, &f->join);
	return f->data;
}

void
fiber_done(const void *data, size_t data_len)
{
	/* not main fiber */
	if (!current->cb) {
		memmove(current->data, data, data_len);
	}

	current->state = DEAD;

	// join waiters
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		struct list_head *pos;
		list_for_each(pos, &current->join) {
			struct fiber *f =
				list_entry(pos, struct fiber, list);
			fiber_wakeup(f);
		}
	}
	_fiber_schedule(DEAD, &dead);
}

/********** private functions ************************************/

static void
_fiber_run(void)
{
	current->state = READY;
	for(;;) {
		current->cb();
		current->state = DEAD;

		// join waiters
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			struct list_head *pos;
			list_for_each(pos, &current->join) {
				struct fiber *f =
					list_entry(pos, struct fiber, list);
				fiber_wakeup(f);
			}
		}
		_fiber_schedule(DEAD, &dead);
	}
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
