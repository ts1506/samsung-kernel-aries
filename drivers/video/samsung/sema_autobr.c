/*  Semaphore Auto Brightness driver
 *  for Samsung Galaxy S I9000
 *
 *   Copyright (c) 2011-2013 stratosk@semaphore.gr
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include "sema_autobr.h"

#define DEF_MIN_BRIGHTNESS		(15)
#define DEF_MAX_BRIGHTNESS		(255)
#define DEF_INSTANT_UPD_THRESHOLD	(30)
#define DEF_MAX_LUX			(2900)
#define DEF_MAX_BR_THRESHOLD		(0)
#define DEF_EFFECT_DELAY_MS		(0)
#define DEF_BLOCK_FW			(1)
#define DEF_ALPHA			(8)	/* between 1 - 9 */
#define NORM				(10)

#define SAMPLE_PERIOD 400000 /* Check every 400000us */

#define DRV_MAX_BRIGHTNESS		(255)
#define DRV_MIN_BRIGHTNESS		(1)
#define DRV_MAX_LUX			(3000)
#define DRV_MAX_UPD_THRESHOLD		(100)
#define DRV_MAX_EFFECT_DELAY		(10)

/* min_brightness: the minimun brightness that will be used
 * max_brightness: the maximum brightness that will be used
 * instant_upd_threshold: the difference threshold that we have to update
 * instantly
 * max_lux: max value from the light sensor
 * effect_delay_ms: delay between step for the fade effect
 * block_fw: block framework brighness updates
 */
static struct sema_ab_tuners {
	unsigned short min_brightness;
	unsigned short max_brightness;
	unsigned short instant_upd_threshold;
	unsigned short max_lux;
	unsigned short max_br_threshold;
	short effect_delay_ms;
	unsigned short block_fw;
	unsigned short alpha;
	unsigned short beta;
	unsigned short norm;
	unsigned short linear;
} sa_tuners = {
	.min_brightness = DEF_MIN_BRIGHTNESS,
	.max_brightness = DEF_MAX_BRIGHTNESS,
	.instant_upd_threshold = DEF_INSTANT_UPD_THRESHOLD,
	.max_lux = DEF_MAX_LUX,
	.max_br_threshold = DEF_MAX_BR_THRESHOLD,
	.effect_delay_ms = DEF_EFFECT_DELAY_MS,
	.block_fw = DEF_BLOCK_FW,
	.alpha  = DEF_ALPHA,
	.beta   = NORM - DEF_ALPHA,
	.norm   = NORM,
	.linear = 1,
};

static void autobr_handler(struct work_struct *w);
DECLARE_DELAYED_WORK(ws, autobr_handler);

static struct sema_ab_info {
	unsigned int current_br;	/* holds the current brightness */
	unsigned int update_br;/* the brightness value that we have to reach */
	unsigned int sum_update_adc;	/* the sum of samples */
	unsigned int cnt;
	unsigned int delay;
	unsigned int old_br;
} sa_info;

/************************** sysfs interface ************************/

static ssize_t show_current_br(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sa_info.current_br);
}

static ssize_t show_min_brightness(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.min_brightness);
}

static ssize_t store_min_brightness(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input < 1 || input > DRV_MAX_BRIGHTNESS ||
		input > sa_tuners.max_brightness)
		return -EINVAL;

	sa_tuners.min_brightness = input;

	return size;
}

static ssize_t show_max_brightness(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.max_brightness);
}

static ssize_t store_max_brightness(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input < 1 || input > DRV_MAX_BRIGHTNESS ||
		input < sa_tuners.min_brightness)
		return -EINVAL;

	sa_tuners.max_brightness = input;

	return size;
}

static ssize_t show_instant_upd_threshold(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.instant_upd_threshold);
}

static ssize_t store_instant_upd_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input < 1 || input > DRV_MAX_UPD_THRESHOLD)
		return -EINVAL;

	sa_tuners.instant_upd_threshold = input;

	return size;
}

static ssize_t show_max_lux(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.max_lux);
}

static ssize_t store_max_lux(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input < 1 || input > (DRV_MAX_LUX + 1000))
		return -EINVAL;

	sa_tuners.max_lux = input;

	return size;
}

static ssize_t show_max_br_threshold(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.max_br_threshold);
}

static ssize_t store_max_br_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input < 1 || input > DRV_MAX_LUX)
		return -EINVAL;

	sa_tuners.max_br_threshold = input;

	return size;
}

static ssize_t show_effect_delay_ms(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%i\n", sa_tuners.effect_delay_ms);
}

static ssize_t store_effect_delay_ms(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	short input;
	int ret;

	ret = sscanf(buf, "%hi", &input);
	if (ret != 1 || input < -1 || input > DRV_MAX_EFFECT_DELAY)
		return -EINVAL;

	sa_tuners.effect_delay_ms = input;

	return size;
}

static ssize_t show_block_fw(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.block_fw);
}

static ssize_t store_block_fw(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input > 1)
		return -EINVAL;

	sa_tuners.block_fw = input;

	if (sa_tuners.block_fw)
		block_bl_update();
	else
		unblock_bl_update();

	return size;
}

static ssize_t show_linear(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%u\n", sa_tuners.linear);
}

static ssize_t store_linear(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned short input;
	int ret;

	ret = sscanf(buf, "%hu", &input);
	if (ret != 1 || input > 1)
		return -EINVAL;

	sa_tuners.linear = input;

	return size;
}

static DEVICE_ATTR(current_br, S_IRUGO, show_current_br, NULL);
static DEVICE_ATTR(min_brightness, S_IRUGO | S_IWUGO , show_min_brightness,
							store_min_brightness);
static DEVICE_ATTR(max_brightness, S_IRUGO | S_IWUGO , show_max_brightness,
							store_max_brightness);
static DEVICE_ATTR(instant_upd_threshold, S_IRUGO | S_IWUGO,
		show_instant_upd_threshold, store_instant_upd_threshold);
static DEVICE_ATTR(max_lux, S_IRUGO | S_IWUGO , show_max_lux, store_max_lux);
static DEVICE_ATTR(max_br_threshold, S_IRUGO | S_IWUGO , show_max_br_threshold,
						store_max_br_threshold);
static DEVICE_ATTR(effect_delay_ms, S_IRUGO | S_IWUGO,
			show_effect_delay_ms, store_effect_delay_ms);
static DEVICE_ATTR(block_fw, S_IRUGO | S_IWUGO , show_block_fw, store_block_fw);
static DEVICE_ATTR(linear, S_IRUGO | S_IWUGO , show_linear, store_linear);

static struct attribute *sema_autobr_attributes[] = {
	&dev_attr_current_br.attr,
	&dev_attr_min_brightness.attr,
	&dev_attr_max_brightness.attr,
	&dev_attr_instant_upd_threshold.attr,
	&dev_attr_max_lux.attr,
	&dev_attr_max_br_threshold.attr,
	&dev_attr_effect_delay_ms.attr,
	&dev_attr_block_fw.attr,
	&dev_attr_linear.attr,
	NULL
};

static struct attribute_group sema_autobr_group = {
	.attrs  = sema_autobr_attributes,
};

static struct miscdevice sema_autobr_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sema_autobr",
};

/************************** sysfs end ************************/

static inline void step_update(void)
{
	int ret;

	(sa_info.current_br < sa_info.update_br) ?
		++sa_info.current_br : --sa_info.current_br;

	ret = bl_update_brightness(sa_info.current_br);
}

static void instant_update(void)
{
	int ret;

	if (sa_info.current_br < sa_info.update_br) {
		if (sa_tuners.effect_delay_ms >= 0) {
			while (sa_info.current_br++ < sa_info.update_br) {
				/* fade in */
				ret = bl_update_brightness(sa_info.current_br);
				msleep(sa_tuners.effect_delay_ms);
		    }
		} else {
			sa_info.current_br = sa_info.update_br;
			ret = bl_update_brightness(sa_info.current_br);
		}
	} else {
		if (sa_tuners.effect_delay_ms >= 0) {
			while (sa_info.current_br-- > sa_info.update_br) {
				/* fade out */
				ret = bl_update_brightness(sa_info.current_br);
				msleep(sa_tuners.effect_delay_ms);
		    }
		} else {
			sa_info.current_br = sa_info.update_br;
			ret = bl_update_brightness(sa_info.current_br);
		}
	}
}

static void autobr_handler(struct work_struct *w)
{
	int diff;
	unsigned int adc;

	adc = ls_get_adcvalue();
	pr_debug("%s: adcvalue: %u", __func__, adc);

	sa_info.sum_update_adc += adc;
	if (++sa_info.cnt < 5)
		goto NEXT_ITER;

	/* Get the average after 5 samples and only then adjust the brightness*/
	sa_info.sum_update_adc = sa_info.sum_update_adc / 5;

	/* Apply max brightness if adc greater than threshold */
	if (sa_tuners.max_br_threshold &&
	    sa_info.sum_update_adc > sa_tuners.max_br_threshold) {
		sa_info.update_br = sa_tuners.max_brightness;
		goto BYPASS;
	}

	/*
	 * Get the adc value from light sensor and
	 * normalize it to 0 - max_brightness scale
	 */
	if (sa_tuners.linear)
		sa_info.update_br = sa_info.sum_update_adc *
				sa_tuners.max_brightness / sa_tuners.max_lux;
	else
		sa_info.update_br = (sa_info.sum_update_adc *
				     sa_info.sum_update_adc *
				     sa_tuners.max_brightness) /
				(sa_tuners.max_lux * sa_tuners.max_lux);

	pr_debug("%s: update_br: %u", __func__, sa_info.update_br);
	pr_debug("%s: old_br: %u", __func__, sa_info.old_br);

	/* Apply low pass filter */
	sa_info.update_br = ((sa_tuners.alpha * sa_info.update_br) +
			     (sa_tuners.beta * sa_info.old_br)) /
			      sa_tuners.norm;

BYPASS:
	sa_info.old_br = sa_info.update_br;

	pr_debug("%s: filtered update_br: %u", __func__, sa_info.update_br);
	pr_debug("%s: update_br: %u, current_br: %u", __func__,
		 sa_info.update_br, sa_info.current_br);

	/* cap the update brightness within the limits */
	if (sa_info.update_br < sa_tuners.min_brightness)
		sa_info.update_br = sa_tuners.min_brightness;
	if (sa_info.update_br > sa_tuners.max_brightness)
		sa_info.update_br = sa_tuners.max_brightness;

	/* the difference between current and update brightness */
	diff = abs(sa_info.current_br - sa_info.update_br);

	if (diff > 1 && diff <= sa_tuners.instant_upd_threshold) {
		/* update one step every SAMPLE_PERIOD * 5 */
		step_update();

	} else if (diff > sa_tuners.instant_upd_threshold) {
		/* instantly update */
		instant_update();
	} else
		sa_info.current_br = sa_info.update_br;

	pr_debug("%s: after current_br: %u", __func__, sa_info.current_br);

	/* reset counters */
	sa_info.sum_update_adc = 0;
	sa_info.cnt = 0;

NEXT_ITER:
	schedule_delayed_work(&ws, sa_info.delay);
}

static void powersave_early_suspend(struct early_suspend *handler)
{
	cancel_delayed_work(&ws);
}

static void powersave_late_resume(struct early_suspend *handler)
{
	schedule_delayed_work(&ws, sa_info.delay);
}

static struct early_suspend _powersave_early_suspend = {
	.suspend = powersave_early_suspend,
	.resume = powersave_late_resume,
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
};

static int autobr_init(void)
{
	misc_register(&sema_autobr_device);
	if (sysfs_create_group(&sema_autobr_device.this_device->kobj,
						&sema_autobr_group) < 0) {
		printk(KERN_ERR "%s sysfs_create_group fail\n", __func__);
		pr_err("Failed to create sysfs group for device (%s)!\n",
						sema_autobr_device.name);
	}

	/* initial values */
	sa_info.current_br = 120;
	sa_info.old_br = 0;
	sa_info.cnt = 0;
	sa_info.delay = usecs_to_jiffies(SAMPLE_PERIOD);

	schedule_delayed_work(&ws, sa_info.delay);

	block_bl_update();

	register_early_suspend(&_powersave_early_suspend);

	printk(KERN_INFO "Semaphore Auto Brightness enabled\n");

	return 0;
}

static void autobr_exit(void)
{
	misc_deregister(&sema_autobr_device);

	cancel_delayed_work(&ws);

	unblock_bl_update();

	unregister_early_suspend(&_powersave_early_suspend);

	printk(KERN_INFO "Semaphore Auto Brightness disabled\n");
}

module_init(autobr_init);
module_exit(autobr_exit);

MODULE_AUTHOR("stratosk@semaphore.gr");
MODULE_DESCRIPTION("Semaphore Auto Brightness driver");
MODULE_LICENSE("GPL");
