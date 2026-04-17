#ifndef __MITEE_TASK_H
#define __MITEE_TASK_H

#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/semaphore.h>

struct optee;
struct tee_context;
struct tee_shm;

#define MITEE_NUM_WORKERS 2

struct mitee_worker {
	struct task_struct *task_ptr;
	struct completion wakeup;
	unsigned long is_busy;
	struct mitee_task *current_task;
	struct optee *optee;
	int id;
};

struct mitee_task {
	int id;
	int state;
	struct completion comp;
	struct tee_context *ctx;
	struct tee_shm *shm;
	int result;
};

int mitee_task_list_init(struct optee *optee);
void mitee_task_list_deinit(struct optee *optee);

struct mitee_task *mitee_task_alloc(struct optee *optee, struct tee_context *ctx, struct tee_shm *shm);
void mitee_task_free(struct optee *optee, struct mitee_task *task);

int mitee_do_call_with_task(struct optee *optee, struct mitee_task *task);

#endif
