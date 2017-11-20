#ifndef FIBER_H
#define FIBER_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct fiber
 * - a structure contains fiber private data.
 */
struct fiber;

/**
 * typedef fiber_cb
 * fiber startup function
 */
typedef void (*fiber_cb)(void);

/**
 * fibers_init() - init fiber system
 *
 * Note: can be call once by session. Main process will
 * be registered as current fiber. The fiber can be cancelled
 * if the other fibers are present.
 */
void fibers_init(void);

/**
 * fiber_create() - create new fiber
 * @cb - fiber startup function
 * @stack - stack for created fiber
 * @stack_size - stack size
 *
 * Note: the function will allocate struct fiber (in the stack)
 * and use the stack while doing switch to the fiber.
 *
 * New fiber creates with 'R' status. One of the following fiber_cede
 * or fiber_schedule calls will switch to the fiber.
 */
struct fiber * fiber_create(fiber_cb cb, void *stack, size_t stack_size);

/**
 * fiber_current - get current fiber handler
 */
struct fiber * fiber_current(void);

/** fiber_status - get fiber status
 *
 * - 'r' - normal processing fiber
 * - 'R' - normal processing fiber that was not run yet
 * - 's' - scheduled fiber (use fiber_wakeup to make the fiber ready)
 * - 'd' - dead fiber (callback was done)
 * - 'c' - cancelled by fiber_cancel
 */
unsigned char fiber_status(const struct fiber *f);

// switch to the other ready fiber (current fiber is ready)
void fiber_cede(void);

// cancel processing the fiber
void fiber_cancel(struct fiber *fiber);

// switch to the other ready fiber (current fiber is sleep)
void fiber_schedule(void);

// wakeup scheduled fiber
void fiber_wakeup(struct fiber *w);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif /* FIBER_H */
