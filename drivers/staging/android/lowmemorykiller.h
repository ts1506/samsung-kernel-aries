/* include/linux/lowmemorykiller.h
 *
 * Copyright (C) 2008 Google, Inc.
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

#ifndef _LINUX_LOWMEMORYKILLER_H
#define _LINUX_LOWMEMORYKILLER_H

#define DEF_DEBUG_LEVEL		(2)
#define DEF_LOWMEM_SIZE		(4)
#define LOWMEM_ARRAY_SIZE	(6)

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			printk(x);			\
	} while (0)

enum lowmem_scan_t {
	LMK_SCAN_OK,
	LMK_SCAN_CONTINUE,
	LMK_SCAN_ABORT,
};

/* Selected task struct */
struct selected_struct {
	struct	task_struct *task;
	int	tasksize;
	short	oom_score_adj;
};

#endif /* _LINUX_LOWMEMORYKILLER_H */
