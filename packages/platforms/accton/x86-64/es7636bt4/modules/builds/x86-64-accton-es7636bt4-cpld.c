/*
 * Copyright (C) Alex Lai <alex_lai@edge-core.com>
 *
 * This module supports the accton cpld that hold the channel select
 * mechanism for other i2c slave devices, such as SFP.
 * This includes the:
 *	 Accton es7636bt4 CPLD1/CPLD2/CPLD3
 *
 * Based on:
 *	pca954x.c from Kumar Gala <galak@kernel.crashing.org>
 * Copyright (C) 2006
 *
 * Based on:
 *	pca954x.c from Ken Harrenstien
 * Copyright (C) 2004 Google, Inc. (Ken Harrenstien)
 *
 * Based on:
 *	i2c-virtual_cb.c from Brian Kuschak <bkuschak@yahoo.com>
 * and
 *	pca9540.c from Jean Delvare <khali@linux-fr.org>.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>

#define I2C_RW_RETRY_COUNT			10
#define I2C_RW_RETRY_INTERVAL			60 /* ms */

static LIST_HEAD(cpld_client_list);
static struct mutex     list_lock;

struct cpld_client_node {
	struct i2c_client *client;
	struct list_head   list;
};

enum cpld_type {
	es7636bt4_cpld1,
	es7636bt4_cpld2,
	es7636bt4_cpld3,
	es7636bt4_cpld_cpu
};

struct es7636bt4_cpld_data {
	enum cpld_type   type;
	struct device   *hwmon_dev;
	struct mutex     update_lock;
};

static const struct i2c_device_id es7636bt4_cpld_id[] = {
	{ "es7636bt4_cpld1", es7636bt4_cpld1 },
	{ "es7636bt4_cpld2", es7636bt4_cpld2 },
	{ "es7636bt4_cpld3", es7636bt4_cpld3 },
	{ "es7636bt4_cpld_cpu", es7636bt4_cpld_cpu },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es7636bt4_cpld_id);

#define TRANSCEIVER_PRESENT_ATTR_ID(index)   	MODULE_PRESENT_##index
#define TRANSCEIVER_RESET_ATTR_ID(index)        MODULE_RESET_##index

enum es7636bt4_cpld_sysfs_attributes {
	CPLD_VERSION,
	ACCESS,
	MODULE_PRESENT_ALL,
	/* transceiver attributes */
	TRANSCEIVER_PRESENT_ATTR_ID(1),
	TRANSCEIVER_PRESENT_ATTR_ID(2),
	TRANSCEIVER_PRESENT_ATTR_ID(3),
	TRANSCEIVER_PRESENT_ATTR_ID(4),
	TRANSCEIVER_PRESENT_ATTR_ID(5),
	TRANSCEIVER_PRESENT_ATTR_ID(6),
	TRANSCEIVER_PRESENT_ATTR_ID(7),
	TRANSCEIVER_PRESENT_ATTR_ID(8),
	TRANSCEIVER_PRESENT_ATTR_ID(9),
	TRANSCEIVER_PRESENT_ATTR_ID(10),
	TRANSCEIVER_PRESENT_ATTR_ID(11),
	TRANSCEIVER_PRESENT_ATTR_ID(12),
	TRANSCEIVER_PRESENT_ATTR_ID(13),
	TRANSCEIVER_PRESENT_ATTR_ID(14),
	TRANSCEIVER_PRESENT_ATTR_ID(15),
	TRANSCEIVER_PRESENT_ATTR_ID(16),
	TRANSCEIVER_PRESENT_ATTR_ID(17),
	TRANSCEIVER_PRESENT_ATTR_ID(18),
	TRANSCEIVER_PRESENT_ATTR_ID(19),
	TRANSCEIVER_PRESENT_ATTR_ID(20),
	TRANSCEIVER_PRESENT_ATTR_ID(21),
	TRANSCEIVER_PRESENT_ATTR_ID(22),
	TRANSCEIVER_PRESENT_ATTR_ID(23),
	TRANSCEIVER_PRESENT_ATTR_ID(24),
	TRANSCEIVER_PRESENT_ATTR_ID(25),
	TRANSCEIVER_PRESENT_ATTR_ID(26),
	TRANSCEIVER_PRESENT_ATTR_ID(27),
	TRANSCEIVER_PRESENT_ATTR_ID(28),
	TRANSCEIVER_PRESENT_ATTR_ID(29),
	TRANSCEIVER_PRESENT_ATTR_ID(30),
	TRANSCEIVER_PRESENT_ATTR_ID(31),
	TRANSCEIVER_PRESENT_ATTR_ID(32),
	TRANSCEIVER_PRESENT_ATTR_ID(33),
	TRANSCEIVER_PRESENT_ATTR_ID(34),
	TRANSCEIVER_PRESENT_ATTR_ID(35),
	TRANSCEIVER_PRESENT_ATTR_ID(36),
	TRANSCEIVER_RESET_ATTR_ID(1),
	TRANSCEIVER_RESET_ATTR_ID(2),
	TRANSCEIVER_RESET_ATTR_ID(3),
	TRANSCEIVER_RESET_ATTR_ID(4),
	TRANSCEIVER_RESET_ATTR_ID(5),
	TRANSCEIVER_RESET_ATTR_ID(6),
	TRANSCEIVER_RESET_ATTR_ID(7),
	TRANSCEIVER_RESET_ATTR_ID(8),
	TRANSCEIVER_RESET_ATTR_ID(9),
	TRANSCEIVER_RESET_ATTR_ID(10),
	TRANSCEIVER_RESET_ATTR_ID(11),
	TRANSCEIVER_RESET_ATTR_ID(12),
	TRANSCEIVER_RESET_ATTR_ID(13),
	TRANSCEIVER_RESET_ATTR_ID(14),
	TRANSCEIVER_RESET_ATTR_ID(15),
	TRANSCEIVER_RESET_ATTR_ID(16),
	TRANSCEIVER_RESET_ATTR_ID(17),
	TRANSCEIVER_RESET_ATTR_ID(18),
	TRANSCEIVER_RESET_ATTR_ID(19),
	TRANSCEIVER_RESET_ATTR_ID(20),
	TRANSCEIVER_RESET_ATTR_ID(21),
	TRANSCEIVER_RESET_ATTR_ID(22),
	TRANSCEIVER_RESET_ATTR_ID(23),
	TRANSCEIVER_RESET_ATTR_ID(24),
	TRANSCEIVER_RESET_ATTR_ID(25),
	TRANSCEIVER_RESET_ATTR_ID(26),
	TRANSCEIVER_RESET_ATTR_ID(27),
	TRANSCEIVER_RESET_ATTR_ID(28),
	TRANSCEIVER_RESET_ATTR_ID(29),
	TRANSCEIVER_RESET_ATTR_ID(30),
	TRANSCEIVER_RESET_ATTR_ID(31),
	TRANSCEIVER_RESET_ATTR_ID(32),
	TRANSCEIVER_RESET_ATTR_ID(33),
	TRANSCEIVER_RESET_ATTR_ID(34),
	TRANSCEIVER_RESET_ATTR_ID(35),
	TRANSCEIVER_RESET_ATTR_ID(36),	
};

/* sysfs attributes for hwmon 
 */
static ssize_t show_status(struct device *dev, struct device_attribute *da,
			char *buf);
static ssize_t show_present_all(struct device *dev, 
				struct device_attribute *da, char *buf);
static ssize_t set_port_reset(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t access(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t show_version(struct device *dev, struct device_attribute *da,
			char *buf);
static int es7636bt4_cpld_read_internal(struct i2c_client *client, u8 reg);
static int es7636bt4_cpld_write_internal(struct i2c_client *client, u8 reg,
					u8 value);

/* transceiver attributes */
#define DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(index) \
	static SENSOR_DEVICE_ATTR(module_present_##index, S_IRUGO, \
				show_status, NULL, MODULE_PRESENT_##index); \
	static SENSOR_DEVICE_ATTR(module_reset_##index, S_IRUGO | S_IWUSR, \
				show_status, set_port_reset, \
				MODULE_RESET_##index);
#define DECLARE_TRANSCEIVER_PRESENT_ATTR(index) \
	&sensor_dev_attr_module_present_##index.dev_attr.attr, \
	&sensor_dev_attr_module_reset_##index.dev_attr.attr

static SENSOR_DEVICE_ATTR(version, S_IRUGO, show_version, NULL, CPLD_VERSION);
static SENSOR_DEVICE_ATTR(access, S_IWUSR, NULL, access, ACCESS);

static SENSOR_DEVICE_ATTR(module_present_all, S_IRUGO, show_present_all, \
			  NULL, MODULE_PRESENT_ALL);

/* transceiver attributes */
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(1);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(2);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(3);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(4);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(5);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(6);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(7);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(8);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(9);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(10);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(11);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(12);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(13);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(14);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(15);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(16);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(17);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(18);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(19);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(20);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(21);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(22);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(23);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(24);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(25);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(26);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(27);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(28);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(29);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(30);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(31);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(32);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(33);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(34);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(35);
DECLARE_TRANSCEIVER_PRESENT_SENSOR_DEVICE_ATTR(36);

static struct attribute *es7636bt4_cpld1_attributes[] = {
	&sensor_dev_attr_version.dev_attr.attr,
	&sensor_dev_attr_access.dev_attr.attr,
	&sensor_dev_attr_module_present_all.dev_attr.attr,
	DECLARE_TRANSCEIVER_PRESENT_ATTR(1),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(2),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(3),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(4),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(5),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(6),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(7),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(8),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(9),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(10),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(11),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(12),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(13),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(14),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(15),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(16),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(17),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(18),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(19),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(20),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(21),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(22),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(23),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(24),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(25),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(26),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(27),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(28),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(29),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(30),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(31),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(32),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(33),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(34),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(35),
	DECLARE_TRANSCEIVER_PRESENT_ATTR(36),
	NULL
};

static const struct attribute_group es7636bt4_cpld1_group = {
	.attrs = es7636bt4_cpld1_attributes,
};

static struct attribute *es7636bt4_cpld2_attributes[] = {
	&sensor_dev_attr_version.dev_attr.attr,
	&sensor_dev_attr_access.dev_attr.attr,
	NULL
};

static const struct attribute_group es7636bt4_cpld2_group = {
	.attrs = es7636bt4_cpld2_attributes,
};

static struct attribute *es7636bt4_cpld3_attributes[] = {
	&sensor_dev_attr_version.dev_attr.attr,
	&sensor_dev_attr_access.dev_attr.attr,
	NULL
};

static const struct attribute_group es7636bt4_cpld3_group = {
	.attrs = es7636bt4_cpld3_attributes,
};

static ssize_t show_present_all(struct device *dev, 
				struct device_attribute *da, char *buf)
{
	int i, status;

	struct i2c_client *client = to_i2c_client(dev);
	struct  es7636bt4_cpld_data *data = i2c_get_clientdata(client);

	u8 regs[] = {0x20, 0x21, 0x22, 0x23, 0x1F};
	u8 values[5] = {0};

	mutex_lock(&data->update_lock);
	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		status = es7636bt4_cpld_read_internal(client, regs[i]);

		if (status < 0)
			goto exit;

		values[i] = ~(u8)status;
	}
	mutex_unlock(&data->update_lock);

	values[3] &= 0xF;
	values[3] = values[3] | (values[4] << 4);
	values[4] = values[4] >> 4; 

	return sprintf(buf, "%.2x %.2x %.2x %.2x %.2x\n", values[0], 
			values[1], values[2], values[3], values[4]);

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t show_status(struct device *dev, struct device_attribute *da,
	     char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct es7636bt4_cpld_data *data = i2c_get_clientdata(client);
	int status = 0;
	u8 reg = 0, mask = 0, revert = 0;

	switch (attr->index) {
	case MODULE_PRESENT_1 ... MODULE_PRESENT_8:
		reg = 0x20;
		mask = 0x1 << (attr->index - MODULE_PRESENT_1);
		break;
	case MODULE_PRESENT_9 ... MODULE_PRESENT_16:
		reg = 0x21;
		mask = 0x1 << (attr->index - MODULE_PRESENT_9);
		break;
	case MODULE_PRESENT_17 ... MODULE_PRESENT_24:
		reg = 0x22;
		mask = 0x1 << (attr->index - MODULE_PRESENT_17);
		break;
	case MODULE_PRESENT_25 ... MODULE_PRESENT_28:
		reg = 0x23;
		mask = 0x1 << (attr->index - MODULE_PRESENT_25);
		break;
	case MODULE_PRESENT_29 ... MODULE_PRESENT_36:
		reg = 0x1F;
		mask = 0x1 << (attr->index - MODULE_PRESENT_29);
		break;   
	case MODULE_RESET_1 ... MODULE_RESET_8:
		reg = 0x4;
		mask = 0x1 << (attr->index - MODULE_RESET_1);
		break;
	case MODULE_RESET_9 ... MODULE_RESET_16:
		reg = 0x5;
		mask = 0x1 << (attr->index - MODULE_RESET_9);
		break;
	case MODULE_RESET_17 ... MODULE_RESET_24:
		reg = 0x6;
		mask = 0x1 << (attr->index - MODULE_RESET_17);
		break;
	case MODULE_RESET_25 ... MODULE_RESET_28:
		reg = 0x7;
		mask = 0x1 << (attr->index - MODULE_RESET_25);
		break;
	case MODULE_RESET_29 ... MODULE_RESET_36:
		reg = 0x8;
		mask = 0x1 << (attr->index - MODULE_RESET_29);
		break;	
	default:
		return 0;
	}

	if (attr->index >= MODULE_PRESENT_1 && 
	    attr->index <= MODULE_PRESENT_36)
		revert = 1;

	if (attr->index >= MODULE_RESET_1 && attr->index <= MODULE_RESET_36)
		revert = 1;

	mutex_lock(&data->update_lock);
	status = es7636bt4_cpld_read_internal(client, reg);
	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n",
			revert ? !(status & mask) : !!(status & mask));

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t set_port_reset(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct es7636bt4_cpld_data *data = i2c_get_clientdata(client);
	long reset;
	int status;
	u8 reg = 0, mask = 0;

	status = kstrtol(buf, 10, &reset);
	if (status)
		return status;

	switch (attr->index) {
	case MODULE_RESET_1 ... MODULE_RESET_8:
		reg = 0x4;
		mask = 0x1 << (attr->index - MODULE_RESET_1);
		break;
	case MODULE_RESET_9 ... MODULE_RESET_16:
		reg = 0x5;
		mask = 0x1 << (attr->index - MODULE_RESET_9);
		break;
	case MODULE_RESET_17 ... MODULE_RESET_24:
		reg = 0x6;
		mask = 0x1 << (attr->index - MODULE_RESET_17);
		break;
	case MODULE_RESET_25 ... MODULE_RESET_28:
		reg = 0x7;
		mask = 0x1 << (attr->index - MODULE_RESET_25);
		break;
	case MODULE_RESET_29 ... MODULE_RESET_36:
		reg = 0x8;
		mask = 0x1 << (attr->index - MODULE_RESET_29);
		break;
	default:
		return -ENXIO;
	}

	/* Read current status */
	mutex_lock(&data->update_lock);
	status = es7636bt4_cpld_read_internal(client, reg);
	if (unlikely(status < 0))
		goto exit;

	/* Update tx_disable status */
	if (reset) {
		status &= ~mask;
	} else {
		status |= mask;
	}

	status = es7636bt4_cpld_write_internal(client, reg, status);
	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}


static ssize_t access(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	int status;
	u32 addr, val;
	struct i2c_client *client = to_i2c_client(dev);
	struct es7636bt4_cpld_data *data = i2c_get_clientdata(client);

	if (sscanf(buf, "0x%x 0x%x", &addr, &val) != 2)
		return -EINVAL;

	if (addr > 0xFF || val > 0xFF)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	status = es7636bt4_cpld_write_internal(client, addr, val);
	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static void es7636bt4_cpld_add_client(struct i2c_client *client)
{
	struct cpld_client_node *node = kzalloc(sizeof(struct cpld_client_node)
					, GFP_KERNEL);

	if (!node) {
		dev_dbg(&client->dev, "Can't allocate cpld_client_node (0x%x)\n",
			client->addr);
		return;
	}

	node->client = client;

	mutex_lock(&list_lock);
	list_add(&node->list, &cpld_client_list);
	mutex_unlock(&list_lock);
}

static void es7636bt4_cpld_remove_client(struct i2c_client *client)
{
	struct list_head    *list_node = NULL;
	struct cpld_client_node *cpld_node = NULL;
	int found = 0;

	mutex_lock(&list_lock);

	list_for_each(list_node, &cpld_client_list)
	{
		cpld_node = list_entry(list_node, struct cpld_client_node,
					list);

		if (cpld_node->client == client) {
			found = 1;
			break;
		}
	}

	if (found) {
		list_del(list_node);
		kfree(cpld_node);
	}

	mutex_unlock(&list_lock);
}

static ssize_t show_version(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int val = 0;
	struct i2c_client *client = to_i2c_client(dev);

	val = i2c_smbus_read_byte_data(client, 0x1);

	if (val < 0)
		dev_dbg(&client->dev, "cpld(0x%x) reg(0x1) err %d\n",
			client->addr, val);

	return sprintf(buf, "%d\n", val);
}

/*
 * I2C init/probing/exit functions
 */
static int es7636bt4_cpld_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adap = to_i2c_adapter(client->dev.parent);
	struct es7636bt4_cpld_data *data;
	int ret = -ENODEV;
	int status;	
	const struct attribute_group *group = NULL;

	if (!i2c_check_functionality(adap, I2C_FUNC_SMBUS_BYTE))
		goto exit;

	data = kzalloc(sizeof(struct es7636bt4_cpld_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	data->type = id->driver_data;

	/* Register sysfs hooks */
	switch (data->type) {
	case es7636bt4_cpld1:
		group = &es7636bt4_cpld1_group;
		break;
	case es7636bt4_cpld2:
		group = &es7636bt4_cpld2_group;
		break;
	case es7636bt4_cpld3:
		group = &es7636bt4_cpld3_group;
		break;
	case es7636bt4_cpld_cpu:
	/* Disable CPLD reset to avoid DUT will be reset.
	 */
	status = es7636bt4_cpld_write_internal(client, 0x3, 0x0); 
	if (status < 0)
		dev_dbg(&client->dev, "cpu_cpld reg 0x65 err %d\n", status);

	default:
		break;
	}

	if (group) {
		ret = sysfs_create_group(&client->dev.kobj, group);
		if (ret)
			goto exit_free;
	}

	es7636bt4_cpld_add_client(client);
	return 0;

exit_free:
	kfree(data);
exit:
	return ret;
}

static int es7636bt4_cpld_remove(struct i2c_client *client)
{
	struct es7636bt4_cpld_data *data = i2c_get_clientdata(client);
	const struct attribute_group *group = NULL;

	es7636bt4_cpld_remove_client(client);

	/* Remove sysfs hooks */
	switch (data->type) {
	case es7636bt4_cpld1:
		group = &es7636bt4_cpld1_group;
		break;
	case es7636bt4_cpld2:
		group = &es7636bt4_cpld2_group;
		break;
	case es7636bt4_cpld3:
		group = &es7636bt4_cpld3_group;
		break;
	default:
		break;
	}

	if (group)
		sysfs_remove_group(&client->dev.kobj, group);

	kfree(data);

	return 0;
}

static int es7636bt4_cpld_read_internal(struct i2c_client *client, u8 reg)
{
	int status = 0, retry = I2C_RW_RETRY_COUNT;

	while (retry) {
		status = i2c_smbus_read_byte_data(client, reg);
		if (unlikely(status < 0)) {
			msleep(I2C_RW_RETRY_INTERVAL);
			retry--;
			continue;
		}
		break;
	}

	return status;
}

static int es7636bt4_cpld_write_internal(struct i2c_client *client, u8 reg,
						u8 value)
{
	int status = 0, retry = I2C_RW_RETRY_COUNT;

	while (retry) {
		status = i2c_smbus_write_byte_data(client, reg, value);
		if (unlikely(status < 0)) {
			msleep(I2C_RW_RETRY_INTERVAL);
			retry--;
			continue;
		}
		break;
	}

	return status;
}

int es7636bt4_cpld_read(unsigned short cpld_addr, u8 reg)
{
	struct list_head   *list_node = NULL;
	struct cpld_client_node *cpld_node = NULL;
	int ret = -EPERM;

	mutex_lock(&list_lock);

	list_for_each(list_node, &cpld_client_list)
	{
		cpld_node = list_entry(list_node, struct cpld_client_node,
					list);

		if (cpld_node->client->addr == cpld_addr) {
			ret = es7636bt4_cpld_read_internal(cpld_node->client,
								reg);
			break;
		}
	}

	mutex_unlock(&list_lock);

	return ret;
}
EXPORT_SYMBOL(es7636bt4_cpld_read);

int es7636bt4_cpld_write(unsigned short cpld_addr, u8 reg, u8 value)
{
	struct list_head   *list_node = NULL;
	struct cpld_client_node *cpld_node = NULL;
	int ret = -EIO;

	mutex_lock(&list_lock);

	list_for_each(list_node, &cpld_client_list)
	{
		cpld_node = list_entry(list_node, struct cpld_client_node,
					list);
		if (cpld_node->client->addr == cpld_addr) {
			ret = es7636bt4_cpld_write_internal(cpld_node->client,
								reg, value);
			break;
		}
	}

	mutex_unlock(&list_lock);

	return ret;
}
EXPORT_SYMBOL(es7636bt4_cpld_write);

static struct i2c_driver es7636bt4_cpld_driver = {
	.driver		= {
		.name	= "es7636bt4_cpld",
		.owner	= THIS_MODULE,
	},
	.probe		= es7636bt4_cpld_probe,
	.remove		= es7636bt4_cpld_remove,
	.id_table	= es7636bt4_cpld_id,
};

static int __init es7636bt4_cpld_init(void)
{
	mutex_init(&list_lock);
	return i2c_add_driver(&es7636bt4_cpld_driver);
}

static void __exit es7636bt4_cpld_exit(void)
{
	i2c_del_driver(&es7636bt4_cpld_driver);
}

MODULE_AUTHOR("Alex Lai <alex_lai@edge-core.com>");
MODULE_DESCRIPTION("Accton I2C CPLD driver");
MODULE_LICENSE("GPL");

module_init(es7636bt4_cpld_init);
module_exit(es7636bt4_cpld_exit);
