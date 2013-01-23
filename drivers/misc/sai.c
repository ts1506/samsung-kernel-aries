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
//#define DEBUG
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

	horizontal screen up, accelerating towards down -
	horizontal screen up, accelerating towards up +
 */

#define ACCEL_LIST_SIZE	(5)
#define DEF_ALPHA	(6)		/* between 1 - 9 */
#define DEF_THRES	(40)
#define NORM		(10)

static struct sai_tuners {
	short alpha;
	short beta;
	short thres;
	short norm;
} sai_tun = {
	.alpha 	= DEF_ALPHA,
	.beta 	= NORM - DEF_ALPHA,
	.thres 	= DEF_THRES,
	.norm	= NORM,
};

struct accel {
	short x;
	short y;
	short z;
	struct list_head list;
};

LIST_HEAD(accels);

struct acceleration {
	short x;
	short y;
	short z;
} accel_buf, grav;

struct sai_inputopen {
	struct input_handle *handle;
	struct work_struct inputopen_work;
};

static struct workqueue_struct *sai_wq;
static struct sai_inputopen inputopen;

static void sai_analyze(void)
{
	struct accel *curr, *previous;
	struct acceleration filt;
#ifdef DEBUG
	int i = 1;
#endif

	/* Save the current measurement to the last node */
	curr = list_entry(accels.prev, struct accel, list);
	pr_debug("%s: before update current: %i,%i,%i\n", __func__, curr->x, 
							curr->y, curr->z);
	curr->x = accel_buf.x;
	curr->y = accel_buf.y;
	curr->z = accel_buf.z;
	pr_debug("%s: after update current: %i,%i,%i\n", __func__, curr->x, 
							curr->y, curr->z);
	
	/* Retrieve the previous measurement */
	previous = list_entry((curr->list).prev, struct accel, list);
	pr_debug("%s: previous: %i,%i,%i\n", __func__, previous->x,
						previous->y, previous->z);

	/* Calculate the gravity using a low pass filter */
	grav.x = ((sai_tun.alpha * grav.x) + (sai_tun.beta * previous->x))
								/ sai_tun.norm;
	grav.y = ((sai_tun.alpha * grav.y) + (sai_tun.beta * previous->y))
								/ sai_tun.norm;
	grav.z = ((sai_tun.alpha * grav.z) + (sai_tun.beta * previous->z))
								/ sai_tun.norm;
	pr_debug("%s: gravity: %i,%i,%i\n", __func__, grav.x, grav.y, grav.z);
	
	/* Remove the gravity from the current to get filtered results */
	filt.x = curr->x - grav.x;
	filt.y = curr->y - grav.y;
	filt.z = curr->z - grav.z;
	pr_debug("%s: filtered: %i,%i,%i\n", __func__, filt.x, filt.y, filt.z);
	
	if (filt.x > sai_tun.thres)
		pr_info("%s: move: %s", __func__, "left");
	if (filt.x < -sai_tun.thres)
                pr_info("%s: move: %s", __func__, "right");
	if (filt.y > sai_tun.thres)
                pr_info("%s: move: %s", __func__, "backward");
	if (filt.y < -sai_tun.thres)
                pr_info("%s: move: %s", __func__, "forward");
	if (filt.z > sai_tun.thres)
                pr_info("%s: move: %s", __func__, "down");
	if (filt.z < -sai_tun.thres)
                pr_info("%s: move: %s", __func__, "up");

#ifdef DEBUG
	/* Print the list */
	list_for_each_entry_reverse(curr, &accels, list) {
		pr_debug("%s: %u: %i,%i,%i\n", __func__, i, curr->x, 
						curr->y, curr->z);
		i++;
	}
#endif

	/* Rotate the list to move the head to next element*/
	list_rotate_left(&accels);
#ifdef DEBUG
	curr = list_entry(accels.prev, struct accel, list);
	pr_debug("%s: move head: %i,%i,%i\n", __func__, curr->x, curr->y, 
								    curr->z);
#endif

	return;
}

static void sai_input_event(struct input_handle *handle,
					    unsigned int type,
					    unsigned int code, int value)
{
	if (type == EV_REL) {
		if (code == REL_X) {
			accel_buf.x = value;
		} else if (code == REL_Y) {
			accel_buf.y = value;
		} else if (code == REL_Z) {
			accel_buf.z = value;
		}
	}

	if (type == EV_SYN && code == SYN_REPORT) {
		pr_debug("%s: %i,%i,%i\n", __func__, accel_buf.x, accel_buf.y,
								  accel_buf.z);
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
		/* Use this value randomly to distinquish the device */
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

/*sysfs interface */
static ssize_t show_alpha(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", sai_tun.alpha);
}

static ssize_t store_alpha(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%uh", &input);
	if (ret != 1 || input < 1 || input > 9)
		return -EINVAL;

	sai_tun.alpha = input;
	sai_tun.beta = sai_tun.norm - sai_tun.alpha;

	return size;
}

static ssize_t show_thres(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", sai_tun.thres);
}

 
static ssize_t store_thres(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%uh", &input);
	if (ret != 1 || input < 1 || input > 100) 
		return -EINVAL;

	sai_tun.thres = input;

	return size;
}

static DEVICE_ATTR(alpha, S_IRUGO | S_IWUGO , show_alpha, store_alpha);
static DEVICE_ATTR(thres, S_IRUGO | S_IWUGO , show_thres, store_thres);
 
static struct attribute *sai_attributes[] = {
	&dev_attr_alpha.attr,
	&dev_attr_thres.attr,
	NULL
};

static struct attribute_group sai_group = {
	.attrs  = sai_attributes,
};

static struct miscdevice sai_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sai",
};

/* sysfs end */

static int sai_init(void)
{
	int rc;
	int i;
	struct accel *tmp;

	misc_register(&sai_device);
	if (sysfs_create_group(&sai_device.this_device->kobj, &sai_group) < 0) {
		pr_err("%s sysfs_create_group fail\n", __func__);
		pr_err("Failed to create sysfs group for device (%s)!\n", 
							sai_device.name);
					
		goto err;
	}  
  
	for (i = ACCEL_LIST_SIZE; i != 0; i--) {
		tmp = kmalloc(sizeof(*tmp), GFP_KERNEL);
		tmp->x = tmp->y = tmp->z = i;
		list_add_tail(&(tmp->list), &accels);
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
	struct accel *tmp, *next;

	misc_deregister(&sai_device);
	
	input_unregister_handler(&sai_input_handler);
	destroy_workqueue(sai_wq);

	list_for_each_entry_safe(tmp, next, &accels, list) {
		list_del(&(tmp->list));
		kfree(tmp);
	}

	printk(KERN_INFO "SAI disabled\n");
}

module_init(sai_init);
module_exit(sai_exit);

MODULE_AUTHOR("stratosk@semaphore.gr");
MODULE_DESCRIPTION("SAI driver");
MODULE_LICENSE("GPL");
