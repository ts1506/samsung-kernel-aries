/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/profile.h>
#include <linux/notifier.h>
#include "lowmemorykiller.h"

static short lowmem_adj[LOWMEM_ARRAY_SIZE] = {
	0,
	1,
	6,
	12,
};
static int lowmem_minfree[LOWMEM_ARRAY_SIZE] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};

static int lowmem_adj_size	= DEF_LOWMEM_SIZE;
static int lowmem_minfree_size	= DEF_LOWMEM_SIZE;
static uint32_t lowmem_debug_level = DEF_DEBUG_LEVEL;

static unsigned long lowmem_deathpending_timeout;
static short min_score_adj;
static struct selected_struct selected;

static void set_min_score_adj(struct shrink_control *sc)
{
	int i;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES);
	int other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM);

	min_score_adj = OOM_SCORE_ADJ_MAX + 1;

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;

	for (i = 0; i < array_size; i++) {
		if (other_free < lowmem_minfree[i] &&
		    other_file < lowmem_minfree[i]) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	if (sc->nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %lu, %x, ofree %d %d, ma %hd\n",
				sc->nr_to_scan, sc->gfp_mask, other_free,
				other_file, min_score_adj);
	return;
}

static enum lowmem_scan_t scan_process(struct task_struct *tsk)
{
	struct task_struct *p;
	short oom_score_adj;
	int tasksize;

	if (tsk->flags & PF_KTHREAD)
		return LMK_SCAN_CONTINUE;

	p = find_lock_task_mm(tsk);
	if (!p)
		return LMK_SCAN_CONTINUE;

	if (test_tsk_thread_flag(p, TIF_MEMDIE) &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout)) {
		task_unlock(p);
		rcu_read_unlock();
		return LMK_SCAN_ABORT;
	}

	oom_score_adj = p->signal->oom_score_adj;
	if (oom_score_adj < min_score_adj) {
		task_unlock(p);
		return LMK_SCAN_CONTINUE;
	}

	tasksize = get_mm_rss(p->mm);
	task_unlock(p);
	if (tasksize <= 0)
		return LMK_SCAN_CONTINUE;

	if (selected.task) {
		if (oom_score_adj < selected.oom_score_adj)
			return LMK_SCAN_CONTINUE;

		if (oom_score_adj == selected.oom_score_adj &&
		    tasksize <= selected.tasksize)
			return LMK_SCAN_CONTINUE;
	}

	selected.task = p;
	selected.tasksize = tasksize;
	selected.oom_score_adj = oom_score_adj;
	lowmem_print(2, "select %d (%s), adj %hd, size %d, to kill\n",
		      p->pid, p->comm, oom_score_adj, tasksize);

	return LMK_SCAN_OK;
}

static inline void kill_selected(void)
{
	lowmem_print(1, "send sigkill to %d (%s), adj %hd, size %d\n",
		      selected.task->pid, selected.task->comm,
		      selected.oom_score_adj, selected.tasksize);

	lowmem_deathpending_timeout = jiffies + HZ;

	send_sig(SIGKILL, selected.task, 0);
	set_tsk_thread_flag(selected.task, TIF_MEMDIE);
}

static int lowmem_shrink(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	int rem = 0;

	set_min_score_adj(sc);

	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (sc->nr_to_scan <= 0 || min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %lu, %x, return %d\n",
			     sc->nr_to_scan, sc->gfp_mask, rem);
		return rem;
	}

	selected.task = NULL;
	selected.tasksize = 0;
	selected.oom_score_adj = min_score_adj;

	rcu_read_lock();

	for_each_process(tsk) {
		if (scan_process(tsk) == LMK_SCAN_ABORT)
			return 0;
	}

	if (selected.task) {
		kill_selected();
		rem -= selected.tasksize;
	}

	lowmem_print(4, "lowmem_shrink %lu, %x, return %d\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();

	return rem;
}

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	register_shrinker(&lowmem_shrinker);
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
}

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");
