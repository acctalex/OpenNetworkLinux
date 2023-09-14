/*
* A FPGA driver for Accton AS7946 74XKB
*
* Copyright (C) 2023 Edgecore Networks Corporation.
* Alex Lai <alex_lai@edge-core.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/hwmon-sysfs.h>
#include <linux/timer.h>
#include <linux/delay.h>

#define I2C_RW_RETRY_COUNT 10

#define FPGA_BUS_NUM 0xB
#define FPGA_ADDR 0x60

#define FPGA_UART_REG_FIFO_CLEAR 0x45
#define FPGA_UART_TOD_OUTPUT_ENABLE 0x42
#define FPGA_UART_TOD_OUT_SELECTION 0x49
#define FPGA_UART_RESET_REG 0x86


#define FPGA_UART_IP_BUS_NUM 0xB
#define FPGA_UART_IP_ADDR 0x62

#define FPGA_UART_TOD_REG_TX_DATA 0x44
#define FPGA_UART_TOD_REG_TX_STATUS 0x48

#define FPGA_UART_TOD_REG_RX_DATA 0xB4
#define FPGA_UART_TOD_REG_RX_STATUS 0x84

#define TMT (1 << 5)
#define TRDY (1 << 6)

#define FPGA_UART_FIFO_DELAY 10 /* UART FIFO waiting time, unit: us */

#define FPGA_UART_READ_DELAY 50

static LIST_HEAD(fpga_client_list);
static struct mutex     list_lock;

struct fpga_client_node {
	struct i2c_client *client;
	struct list_head   list;
};

enum fpga_type {
	as7946_74xkb_fpga,
	as7946_74xkb_uartip,
};

struct as7946_74xkb_fpga_data {
	enum fpga_type   type;
	struct device   *hwmon_dev;
	struct mutex     update_lock;
};

static const struct i2c_device_id as7946_74xkb_fpga_id[] = {
	{ "as7946_74xkb_fpga", as7946_74xkb_fpga },
	{ "as7946_74xkb_uartip", as7946_74xkb_uartip },
	{ }
};
MODULE_DEVICE_TABLE(i2c, as7946_74xkb_fpga_id);

enum as7946_74xkb_uartip_sysfs_attributes{
	TOD_TX_DATA,
};

/* sysfs attributes for hwmon 
 */
static ssize_t set_tod_tx_data(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);

int as7946_74xkb_fpga_read(int bus_num, unsigned short fpga_addr, u8 reg);
int as7946_74xkb_fpga_write(int bus_num, unsigned short fpga_addr, u8 reg, 
				u8 value);

static SENSOR_DEVICE_ATTR(tod_tx_data, S_IWUSR, NULL, set_tod_tx_data, 
				TOD_TX_DATA);

static struct attribute *as7946_74xkb_fpga_attributes[] = {
	NULL
};

static const struct attribute_group as7946_74xkb_fpga_group = {
	.attrs = as7946_74xkb_fpga_attributes,
};

static struct attribute *as7946_74xkb_uartip_attributes[] = {
	&sensor_dev_attr_tod_tx_data.dev_attr.attr,
	NULL
};

static const struct attribute_group as7946_74xkb_uartip_group = {
	.attrs = as7946_74xkb_uartip_attributes,
};

static ssize_t set_tod_tx_data(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct as7946_74xkb_fpga_data *data = i2c_get_clientdata(client);

	int status = 0;
	int val, i, j;
	u8 bus, addr, reg;

	char *tod_tx_data = NULL;

	tod_tx_data = kzalloc(count, GFP_KERNEL);
	if (!tod_tx_data)
		return -ENOMEM;

	memcpy(tod_tx_data, buf, count);

	mutex_lock(&data->update_lock);

	switch(attr->index){
	case TOD_TX_DATA:
		// Clear FIFO
		bus = FPGA_BUS_NUM;
		addr = FPGA_ADDR;
		reg = FPGA_UART_REG_FIFO_CLEAR;
		status = val = as7946_74xkb_fpga_read(bus, addr, reg);
		if (unlikely(status < 0))
			goto exit;

		val &= 0xF0;
		status = as7946_74xkb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		val |= 0x3;
		status = as7946_74xkb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		// Enable ToD_RX into FIFO
		status = val = as7946_74xkb_fpga_read(bus, addr, reg);
		if (unlikely(status < 0))
			goto exit;		

		val &= ~0x8;
		val |= 0x8;
		status = as7946_74xkb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;
		
		// TOD OUTPUT Sel and Enable
		reg = FPGA_UART_TOD_OUT_SELECTION;
		val = 0x2;
		status = as7946_74xkb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;
		
		reg = FPGA_UART_TOD_OUTPUT_ENABLE;
		val = 0x2;
		status = as7946_74xkb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		// TOD OUTPUT
		bus = FPGA_UART_IP_BUS_NUM;
		addr = FPGA_UART_IP_ADDR;

		for(i = 0; i < count-1; i++) {

			reg = FPGA_UART_TOD_REG_TX_STATUS;
			for(j = 0; j < I2C_RW_RETRY_COUNT; j++){
				status = val = as7946_74xkb_fpga_read(bus, 
						addr, reg);
				if (unlikely(status < 0))
					goto exit;

				if(!(val & (TRDY | TMT))){
					msleep(FPGA_UART_FIFO_DELAY);
					continue;
				}
				break;
			}

			if(j >= I2C_RW_RETRY_COUNT){
				status = -EBUSY;
				goto exit;
			}

			reg = FPGA_UART_TOD_REG_TX_DATA;
			status = as7946_74xkb_fpga_write(bus, addr, reg, 
					(unsigned char)(*(tod_tx_data+i)));

			if (unlikely(status < 0))
				goto exit;
		}

		status = count;
		break;
	}

exit:
	mutex_unlock(&data->update_lock);

	kfree(tod_tx_data);
	return status;
}

static void as7946_74xkb_fpga_add_client(struct i2c_client *client)
{
	struct fpga_client_node *node = 
		kzalloc(sizeof(struct fpga_client_node), GFP_KERNEL);

	if (!node) {
		dev_dbg(&client->dev, "Can't allocate fpga_client_node (0x%x)\n", client->addr);
		return;
	}

	node->client = client;

	mutex_lock(&list_lock);
	list_add(&node->list, &fpga_client_list);
	mutex_unlock(&list_lock);
}

static void as7946_74xkb_fpga_remove_client(struct i2c_client *client)
{
	struct list_head    *list_node = NULL;
	struct fpga_client_node *fpga_node = NULL;
	int found = 0;

	mutex_lock(&list_lock);

	list_for_each(list_node, &fpga_client_list) {
		fpga_node = list_entry(list_node, struct fpga_client_node, 
					list);

		if (fpga_node->client == client) {
			found = 1;
			break;
		}
	}

	if (found) {
		list_del(list_node);
		kfree(fpga_node);
	}

	mutex_unlock(&list_lock);
}

/*
 * I2C init/probing/exit functions
 */
static int as7946_74xkb_fpga_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adap = to_i2c_adapter(client->dev.parent);
	struct as7946_74xkb_fpga_data *data;
	int ret = -ENODEV;
	const struct attribute_group *group = NULL;

	if (!i2c_check_functionality(adap, I2C_FUNC_SMBUS_BYTE))
		goto exit;

	data = kzalloc(sizeof(struct as7946_74xkb_fpga_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	data->type = id->driver_data;

	/* Register sysfs hooks */
	switch (data->type) {
	case as7946_74xkb_fpga:
		group = &as7946_74xkb_fpga_group;
		break;
	case as7946_74xkb_uartip:
		group = &as7946_74xkb_uartip_group;
		break;
	default:
		break;
	}

	if (group) {
		ret = sysfs_create_group(&client->dev.kobj, group);
		if (ret)
			goto exit_free;
	}

	as7946_74xkb_fpga_add_client(client);
	return 0;

exit_free:
	kfree(data);
exit:
	return ret;
}

static int as7946_74xkb_fpga_remove(struct i2c_client *client)
{
	struct as7946_74xkb_fpga_data *data = i2c_get_clientdata(client);
	const struct attribute_group *group = NULL;

	as7946_74xkb_fpga_remove_client(client);

	/* Remove sysfs hooks */
	switch (data->type) {
	case as7946_74xkb_fpga:
		group = &as7946_74xkb_fpga_group;
		break;
	case as7946_74xkb_uartip:
		group = &as7946_74xkb_uartip_group;
		break;
	default:
		break;
	}

	if (group)
		sysfs_remove_group(&client->dev.kobj, group);

	kfree(data);

	return 0;
}

int as7946_74xkb_fpga_read(int bus_num, unsigned short fpga_addr, u8 reg)
{
	struct list_head   *list_node = NULL;
	struct fpga_client_node *fpga_node = NULL;
	int ret = -EPERM;

	mutex_lock(&list_lock);

	list_for_each(list_node, &fpga_client_list)
	{
		fpga_node = list_entry(list_node, struct fpga_client_node, 
					list);

		if (fpga_node->client->addr == fpga_addr
			&& fpga_node->client->adapter->nr == bus_num) {
			ret = i2c_smbus_read_byte_data(fpga_node->client, reg);
			break;
		}
	}

	mutex_unlock(&list_lock);

	return ret;
}

int as7946_74xkb_fpga_write(int bus_num, unsigned short fpga_addr, u8 reg, u8 value)
{
	struct list_head *list_node = NULL;
	struct fpga_client_node *fpga_node = NULL;
	int ret = -EIO;

	mutex_lock(&list_lock);

	list_for_each(list_node, &fpga_client_list) {
		fpga_node = list_entry(list_node, struct fpga_client_node, list);

		if (fpga_node->client->addr == fpga_addr
			&& fpga_node->client->adapter->nr == bus_num) {
			ret = i2c_smbus_write_byte_data(fpga_node->client, 
							reg, value);
			break;
		}
	}

	mutex_unlock(&list_lock);

	return ret;
}

static struct i2c_driver as7946_74xkb_fpga_driver = {
	.driver         = {
		.name   = "as7946_74xkb_fpga",
		.owner  = THIS_MODULE,
	},
	.probe          = as7946_74xkb_fpga_probe,
	.remove         = as7946_74xkb_fpga_remove,
	.id_table       = as7946_74xkb_fpga_id,
};


static int __init as7946_74xkb_fpga_init(void)
{
	mutex_init(&list_lock);
	return i2c_add_driver(&as7946_74xkb_fpga_driver);
}

static void __exit as7946_74xkb_fpga_exit(void)
{
	i2c_del_driver(&as7946_74xkb_fpga_driver);
}

MODULE_AUTHOR("Alex Lai <alex_lai@edge-core.com>");
MODULE_DESCRIPTION("Accton I2C FPGA driver");
MODULE_LICENSE("GPL");

module_init(as7946_74xkb_fpga_init);
module_exit(as7946_74xkb_fpga_exit);
