/*  SAI driver
 *  for Samsung Galaxy S I9000
 *
 *   Copyright (c) 2012-2013 stratosk@semaphore.gr
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/vmalloc.h>

/*
 *	horizontal screen up 0, 0, 255
 *	horizantal screen down 0, 0, -255
 *	right side up 255, 0 ,0
 *	right side down -255, 0 ,0
 *	top side up 0, 255, 0
 *	top side down 0, -255 ,0

	pickup left ear 50,220,0
	pickup right ear -80, 230, 20

	Range -512 to 511
 */
struct accel_list {
	short x;
	short y;
	short z;
	struct list_head list;
} accel;

struct accel_list *head;

struct sai_inputopen {
	struct input_handle *handle;
	struct work_struct inputopen_work;
};

static struct workqueue_struct *sai_wq;
static struct sai_inputopen inputopen;

static void sai_analyze(void)
{
	struct accel_list *tmp;
	struct list_head *pos;
	int i = 1;

	pr_info("%s: head: %i,%i,%i\n", __func__, head->x, head->y, head->z);
	pos = &(head->list);
	head = list_entry(pos->next, struct accel_list, list);
	pr_info("%s: move head: %i,%i,%i\n", __func__, head->x, head->y, head->z);
	list_for_each(pos, &accel.list) {
		tmp = list_entry(pos, struct accel_list, list);
		pr_info("%s: %u: %i,%i,%i\n", __func__, i, tmp->x, tmp->y, tmp->z);
		i++;		
	}
/*	daccel.x = accel.x - old_accel.x;
	daccel.y = accel.y - old_accel.y;
	daccel.z = accel.z - old_accel.z;
	pr_info("%s: %i,%i,%i\n", __func__, daccel.x, daccel.y, daccel.z);
*/	return;
}

static void sai_input_event(struct input_handle *handle,
					    unsigned int type,
					    unsigned int code, int value)
{
	/*struct input_dev *dev = handle->private;
	if (!handle)
		return;
	if (handle->dev.name != "accelerometer_sensor")
		return;
	dev->name = "accelerometer_sensor";
	dev->id.bustype = BUS_I2C;

	if (handle->dev->name) {
		pr_info("%s: %s\n", __func__, handle->dev->name);
		if (strcmp(handle->dev->name, "accelerometer_sensor") == 0)
			pr_info("%s: accelerometer_sensor\n", __func__);
		else
			pr_info("%s: something else\n", __func__);
	} else
		pr_info("%s: device name null\n", __func__);
	*/

	if (type == EV_REL) {
		if (code == REL_X) {
			head->x = value;
		} else if (code == REL_Y) {
			head->y = value;
		} else if (code == REL_Z) {
			head->z = value;
		}
	}

	if (type == EV_SYN && code == SYN_REPORT) {
		pr_info("%s: %i,%i,%i\n", __func__, head->x, head->y, head->z);
		sai_analyze();
	}
}

static void sai_input_open(struct work_struct *w)
{
	struct sai_inputopen *io =
		container_of(w, struct sai_inputopen,
			     inputopen_work);
	int error;

	error = input_open_device(io->handle);
	if (error)
		input_unregister_handle(io->handle);
}

static int sai_input_connect(struct input_handler *handler,
					     struct input_dev *dev,
					     const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	pr_info("%s: connect to %s\n", __func__, dev->name);
	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "sai";

	error = input_register_handle(handle);
	if (error)
		goto err;

	inputopen.handle = handle;
	queue_work(sai_wq, &inputopen.inputopen_work);
	return 0;
err:
	kfree(handle);
	return error;
}

static void sai_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id sai_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_RELBIT |
				INPUT_DEVICE_ID_MATCH_PRODUCT,
		.evbit = { BIT_MASK(EV_REL) },
		.relbit = { BIT_MASK(REL_X) | BIT_MASK(REL_Y)
					    | BIT_MASK(REL_Z) },
		.product = 7,
	},
	{ },
};

static struct input_handler sai_input_handler = {
	.event          = sai_input_event,
	.connect        = sai_input_connect,
	.disconnect     = sai_input_disconnect,
	.name           = "sai",
	.id_table       = sai_ids,
};

static int sai_init(void)
{
	int rc;
	int i;
	struct accel_list *tmp;

	INIT_LIST_HEAD(&accel.list);
	for (i = 5; i != 0; i--) {
		tmp = (struct accel_list *)vmalloc(sizeof(struct accel_list));

		list_add(&(tmp->list), &(accel.list));
		head = tmp;
	}	

	sai_wq = alloc_workqueue("ksai", 0, 1);
	if (!sai_wq)
		goto err;
	INIT_WORK(&inputopen.inputopen_work, sai_input_open);
	rc = input_register_handler(&sai_input_handler);
	if (rc)
		pr_warn("%s: failed to register input handler\n",
			__func__);

	printk(KERN_INFO "SAI enabled\n");

	return 0;
err:
	return -ENOMEM;
}

static void sai_exit(void)
{
	input_unregister_handler(&sai_input_handler);
	destroy_workqueue(sai_wq);
	printk(KERN_INFO "SAI disabled\n");
}

module_init(sai_init);
module_exit(sai_exit);

MODULE_AUTHOR("stratosk@semaphore.gr");
MODULE_DESCRIPTION("SAI driver");
MODULE_LICENSE("GPL");
