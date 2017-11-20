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

/** fiber_cede - switch to the other processing fiber
 *
 * return immediately if there is no one other processing fibers
 */
void fiber_cede(void);

/** fiber_cancel - cancel processing fiber
 *
 * Note: the function can use to cancel current fiber.
 */
void fiber_cancel(struct fiber *fiber);

/** fiber_schedule - switch to the other ready fiber,
 * marks current fiber as scheduled.
 *
 */
void fiber_schedule(void);

/** fiber_wakeup - wakeup scheduled fiber
 *
 * Note: the function can be call inside interrupt handler.
 */
void fiber_wakeup(struct fiber *w);

/** fiber_join - wait until fiber function done
 *
 * Note: return NULL if:
 *
 * - try join itself
 * - join main fiber
 *
 * main fiber can be cancelled, can join, can done by fiber_done,
 * but it uses default CPU's stack, so there is no place to store
 * done data.
 */
const void * fiber_join(struct fiber *f);

/** fiber_done - done current fiber
 *
 * @data - data to copy to output buffer
 * @data_len - length of @data
 *
 * Note: output buffer is placed on fiber's stack.
 * so stack have to have enough size to contain
 * the data and struct fiber (~16 bytes).
 *
 * Note: main fiber does not copy the data.
 */
void fiber_done(const void *data, size_t data_len);

/** fiber_unlink - unlink fiber from internal lists
 *
 * @f - fiber
 *
 * Each fiber is placed in internal fiber's lists (ready, zombies, etc).
 * If You joined the fiber, You can use it's stack area after the call.
 */
void fiber_unlink(struct fiber *);
#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif /* FIBER_H */
