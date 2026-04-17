#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include "optee_private.h"
#include "mitee_task.h"

extern int optee_ffa_yielding_call_from_worker(struct tee_context *ctx, struct tee_shm *shm);

static int mitee_worker_fn(void *data)
{
	struct mitee_worker *worker = data;
	struct mitee_task *task;
    
	while (!kthread_should_stop()) {
		wait_for_completion_interruptible(&worker->wakeup);
		if (kthread_should_stop())
			break;

		task = worker->current_task;
		if (task) {
			task->result = optee_ffa_yielding_call_from_worker(task->ctx, task->shm);
			complete(&task->comp);
			worker->current_task = NULL;
		}
	}
	return 0;
}

int mitee_task_list_init(struct optee *optee)
{
	int i;
	
	idr_init(&optee->task_idr);
	mutex_init(&optee->task_mutex);
	sema_init(&optee->call_sem, num_online_cpus());

	optee->workers = kcalloc(MITEE_NUM_WORKERS, sizeof(struct mitee_worker), GFP_KERNEL);
	if (!optee->workers)
		return -ENOMEM;

	for (i = 0; i < MITEE_NUM_WORKERS; i++) {
		optee->workers[i].id = i;
		optee->workers[i].optee = optee;
		optee->workers[i].is_busy = 0;
		optee->workers[i].current_task = NULL;
		init_completion(&optee->workers[i].wakeup);
		
		optee->workers[i].task_ptr = kthread_create_on_node(
			mitee_worker_fn, &optee->workers[i], NUMA_NO_NODE,
			"mitee_worker/%d", i);

		if (IS_ERR(optee->workers[i].task_ptr)) {
			pr_err("failed to create mitee_worker/%d\n", i);
			return PTR_ERR(optee->workers[i].task_ptr);
		}
		wake_up_process(optee->workers[i].task_ptr);
	}

	return 0;
}

void mitee_task_list_deinit(struct optee *optee)
{
	int i;

	if (!optee->workers)
		return;

	for (i = 0; i < MITEE_NUM_WORKERS; i++) {
		if (optee->workers[i].task_ptr)
			kthread_stop(optee->workers[i].task_ptr);
	}
	kfree(optee->workers);
	optee->workers = NULL;
	idr_destroy(&optee->task_idr);
}

struct mitee_task *mitee_task_alloc(struct optee *optee, struct tee_context *ctx, struct tee_shm *shm)
{
	struct mitee_task *task;
	int id;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return ERR_PTR(-ENOMEM);

	task->ctx = ctx;
	task->shm = shm;
	init_completion(&task->comp);

	mutex_lock(&optee->task_mutex);
	id = idr_alloc(&optee->task_idr, task, 1, 0, GFP_KERNEL);
	mutex_unlock(&optee->task_mutex);

	if (id < 0) {
		kfree(task);
		return ERR_PTR(id);
	}
	task->id = id;
	return task;
}

void mitee_task_free(struct optee *optee, struct mitee_task *task)
{
	if (!task)
		return;

	mutex_lock(&optee->task_mutex);
	idr_remove(&optee->task_idr, task->id);
	mutex_unlock(&optee->task_mutex);
	kfree(task);
}

int mitee_do_call_with_task(struct optee *optee, struct mitee_task *task)
{
	int i;
	int worker_idx = -1;
	int ret;

	if (!optee->workers)
		return -ENODEV;

	if (down_interruptible(&optee->call_sem))
		return -ERESTARTSYS;

	while (worker_idx < 0) {
		for (i = 0; i < MITEE_NUM_WORKERS; i++) {
			if (!test_and_set_bit(0, &optee->workers[i].is_busy)) {
				worker_idx = i;
				break;
			}
		}
		if (worker_idx < 0) {
			cpu_relax();
		}
	}

	optee->workers[worker_idx].current_task = task;
	complete(&optee->workers[worker_idx].wakeup);

	wait_for_completion(&task->comp);
	ret = task->result;

	clear_bit(0, &optee->workers[worker_idx].is_busy);
	up(&optee->call_sem);

	return ret;
}
