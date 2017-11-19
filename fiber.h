#ifndef FIBER_H
#define FIBER_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fiber;
typedef void (*fiber_cb)(void);

void fibers_init(void);

// create fiber that has state `ready`
struct fiber * fiber_create(fiber_cb cb, void *stack, size_t stack_size);

// return current fiber
struct fiber * fiber_current(void);

// switch to the other ready fiber (current fiber is ready)
void fiber_cede(void);

// cancel processing the fiber
void fiber_cancel(struct fiber *fiber);

// switch to the other ready fiber (current fiber is sleep)
void fiber_schedule(void);

void fiber_wakeup(struct fiber *w);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif /* FIBER_H */
