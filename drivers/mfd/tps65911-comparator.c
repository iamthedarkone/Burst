/*
 * tps65910.c  --  TI TPS6591x
 *
 * Copyright 2010 Texas Instruments Inc.
 *
 * Author: Jorge Eduardo Candelaria <jedu@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/mfd/tps65910.h>

#define COMP					0
#define COMP1					1
#define COMP2					2

/* Comparator 1 voltage selection table in milivolts */
static const u16 COMP_VSEL_TABLE[] = {
	0, 2500, 2500, 2500, 2500, 2550, 2600, 2650,
	2700, 2750, 2800, 2850, 2900, 2950, 3000, 3050,
	3100, 3150, 3200, 3250, 3300, 3350, 3400, 3450,
	3500,
};

struct comparator {
	const char *name;
	int reg;
	int uV_max;
	const u16 *vsel_table;
};

static struct comparator tps_comparators[] = {
	{
		.name = "COMP1",
		.reg = TPS65911_VMBCH,
		.uV_max = 3500,
		.vsel_table = COMP_VSEL_TABLE,
	},
	{
		.name = "COMP2",
		.reg = TPS65911_VMBCH2,
		.uV_max = 3500,
		.vsel_table = COMP_VSEL_TABLE,
	},
};

static int comp_threshold_set(struct tps65910 *tps65910, int id, int voltage)
{
	struct comparator tps_comp = tps_comparators[id];
	int curr_voltage = 0;
	int ret;
	u8 index = 0, val;

	if (id == COMP)
		return 0;

	while (curr_voltage < tps_comp.uV_max) {
		curr_voltage = tps_comp.vsel_table[index];
		if (curr_voltage >= voltage)
			break;
		else if (curr_voltage < voltage)
			index ++;
	}

	if (curr_voltage > tps_comp.uV_max)
		return -EINVAL;

	val = index << 1;
	ret = tps65910->write(tps65910, tps_comp.reg, 1, &val);

	return ret;
}

static int comp_threshold_get(struct tps65910 *tps65910, int id)
{
	struct comparator tps_comp = tps_comparators[id];
	int ret;
	u8 val;

	if (id == COMP)
		return 0;

	ret = tps65910->read(tps65910, tps_comp.reg, 1, &val);
	if (ret < 0)
		return ret;

	val >>= 1;
	return tps_comp.vsel_table[val];
}

static ssize_t comp_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tps65910 *tps65910 = dev_get_drvdata(dev->parent);
	struct attribute comp_attr = attr->attr;
	int id, uVolt;

	if (!strcmp(comp_attr.name, "comp1_threshold"))
		id = COMP1;
	else if (!strcmp(comp_attr.name, "comp2_threshold"))
		id = COMP2;
	else
		return -EINVAL;

	uVolt = comp_threshold_get(t