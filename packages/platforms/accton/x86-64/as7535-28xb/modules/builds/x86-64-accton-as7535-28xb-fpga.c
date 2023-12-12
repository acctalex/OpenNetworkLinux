/*
* A FPGA driver for Accton AS7535 28XB
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

#define FPGA_UART_REG_FIFO_CLEAR 0x49
#define FPGA_UART_TOD_OUTPUT_ENABLE 0x42
#define FGPA_UART_GPS_OUT_SELECTION 0x45
#define FPGA_UART_TOD_OUT_SELECTION 0x46

#define FPGA_UART_IP_BUS_NUM 0xB
#define FPGA_UART_IP_ADDR 0x62

#define FPGA_UART_FIFO_SIZE 4096

#define FPGA_UART_TOD_REG_TX_DATA 0x44
#define FPGA_UART_TOD_REG_TX_STATUS 0x48

#define FPGA_UART_TOD_REG_RX_DATA 0xB4
#define FPGA_UART_TOD_REG_RX_STATUS 0x84

#define FPGA_UART_GPS_REG_TX_STATUS 0x28
#define FPGA_UART_GPS_REG_TX_DATA 0x24
#define FPGA_UART_GPS_REG_RX_STATUS 0x64
#define FPGA_UART_GPS_REG_RX_DATA 0xA4

#define TMT (1 << 5)
#define TRDY (1 << 6)

#define GPS_RX_FIFO_EMPTY (0x2)

#define FPGA_UART_FIFO_DELAY 10 /* UART FIFO waiting time, unit: us */

#define FPGA_UART_READ_DELAY 50

#define MAX_TSIP_PACKET_LENGTH 175

#define DLE (0x10)
#define ETX (0x03)

#define CURRENT_TIME_REQ (0x21)
#define CURRENT_TIME_BYTE_NUM (0xE)

#define SET_PORT_CONFIG (0xBC)
#define PORT1 (0x0)
#define PORT_COFIG_BYTE_NUM (0xE)
#define BAUT_RATE (0x6)
#define DATA_BITS_8 (0x3)
#define PARITY_ODD (0x1)
#define STOP_BIT_1 (0x0)
#define IN_NMEA (0x4)
#define OUT_NMEA (0x4)

#define SET_NMEA_INTERVAL_MSGMSK (0x7A)
#define NMEA_INTERVAL_MSGMSK_SUBID (0x00)
#define GGA_MSK (1 << 0)
#define GLL_MSK (1 << 1)
#define VTG_MSK (1 << 2)
#define GSV_MSK (1 << 3)
#define GSA_MSK (1 << 4)
#define ZDA_MSK (1 << 5)
#define RMC_MSK (1 << 7)
#define GST_MSK (1 << 10)
#define NMEA_INTERVAL_MSGMSK_BYTE_NUM (0xA)

#define FPGA_TO_UART_IP0 (0x2)

static LIST_HEAD(fpga_client_list);
static struct mutex     list_lock;

struct fpga_client_node {
	struct i2c_client *client;
	struct list_head   list;
};

enum fpga_type {
	as7535_28xb_fpga,
	as7535_28xb_uartip,
};

struct as7535_28xb_fpga_data {
	enum fpga_type   type;
	struct device   *hwmon_dev;
	struct mutex     update_lock;
};

static const struct i2c_device_id as7535_28xb_fpga_id[] = {
	{ "as7535_28xb_fpga", as7535_28xb_fpga },
	{ "as7535_28xb_uartip", as7535_28xb_uartip },
	{ }
};
MODULE_DEVICE_TABLE(i2c, as7535_28xb_fpga_id);

enum as7535_28xb_uartip_sysfs_attributes{
	TOD_TX_DATA,
	GPS_RX_FIFO,
	GPS_TIME,
	PORT_CONFIG,
	NMEA_PROTOCOL,
	NMEA_INTERVAL_MSGMSK,
};

/* sysfs attributes for hwmon 
 */
static ssize_t set_tod_tx_data(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);

static ssize_t set_to_nmea(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);

static ssize_t set_nmea_interval_msgmsk(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);

static ssize_t get_gnss_data(struct device *dev, struct device_attribute *da,
			 char *buf);

int as7535_28xb_fpga_read(int bus_num, unsigned short fpga_addr, u8 reg);
int as7535_28xb_fpga_write(int bus_num, unsigned short fpga_addr, u8 reg, 
				u8 value);

static SENSOR_DEVICE_ATTR(tod_tx_data, S_IWUSR, NULL, set_tod_tx_data, 
				TOD_TX_DATA);

static SENSOR_DEVICE_ATTR(gps_rx_fifo, S_IRUGO, get_gnss_data, NULL, 
				GPS_RX_FIFO);

static SENSOR_DEVICE_ATTR(gps_time, S_IRUGO, get_gnss_data, NULL, 
				GPS_TIME);

static SENSOR_DEVICE_ATTR(port_config, S_IRUGO, get_gnss_data, NULL, 
				PORT_CONFIG);

static SENSOR_DEVICE_ATTR(nmea_protocol, S_IWUSR, NULL, set_to_nmea, 
				NMEA_PROTOCOL);

static SENSOR_DEVICE_ATTR(nmea_interval_msgmsk, S_IWUSR, NULL, 
			set_nmea_interval_msgmsk, NMEA_INTERVAL_MSGMSK);

static struct attribute *as7535_28xb_fpga_attributes[] = {
	NULL
};

static const struct attribute_group as7535_28xb_fpga_group = {
	.attrs = as7535_28xb_fpga_attributes,
};

static struct attribute *as7535_28xb_uartip_attributes[] = {
	&sensor_dev_attr_tod_tx_data.dev_attr.attr,
	&sensor_dev_attr_gps_rx_fifo.dev_attr.attr,
	&sensor_dev_attr_gps_time.dev_attr.attr,
	&sensor_dev_attr_port_config.dev_attr.attr,
	&sensor_dev_attr_nmea_protocol.dev_attr.attr,
	&sensor_dev_attr_nmea_interval_msgmsk.dev_attr.attr,
	NULL
};

static const struct attribute_group as7535_28xb_uartip_group = {
	.attrs = as7535_28xb_uartip_attributes,
};


static int fpga_gps_uart_output_sel(u8 sel)
{
	return as7535_28xb_fpga_write(FPGA_BUS_NUM, FPGA_ADDR,
			FGPA_UART_GPS_OUT_SELECTION, sel);
}

static int gps_rx_into_fifo(u8 enable)
{
	int status = 0;
	int val = 0;

	status = val = as7535_28xb_fpga_read(FPGA_BUS_NUM, FPGA_ADDR, 
		FPGA_UART_REG_FIFO_CLEAR);

	if(enable)
		val |= (1 << 2);
	else
		val &= ~(1 << 2);

	status |= as7535_28xb_fpga_write(FPGA_BUS_NUM, FPGA_ADDR, 
			FPGA_UART_REG_FIFO_CLEAR, val);

	return status;
}

static int uart_fifo_clear(void)
{
	int status = 0;
	int val = 0;

	status = val = as7535_28xb_fpga_read(FPGA_BUS_NUM, FPGA_ADDR, 
		FPGA_UART_REG_FIFO_CLEAR);

	val &= 0xF0;
	status |= as7535_28xb_fpga_write(FPGA_BUS_NUM, FPGA_ADDR, 
			FPGA_UART_REG_FIFO_CLEAR, val);

	val |= 0x3;
	status |= as7535_28xb_fpga_write(FPGA_BUS_NUM, FPGA_ADDR, 
			FPGA_UART_REG_FIFO_CLEAR, val);

	return status;
}

static int send_gps_packet_to_gnss(u8 packet[], u8 packet_num)
{
	int status = 0;
	int i = 0, j = 0;

	for(i = 0; i < packet_num; i++) {
		for(j = 0; j < I2C_RW_RETRY_COUNT; j++){
			status = as7535_28xb_fpga_read(
				FPGA_UART_IP_BUS_NUM, FPGA_UART_IP_ADDR
				, FPGA_UART_GPS_REG_TX_STATUS);

			if (unlikely(status < 0))
				return status;

			if(!(status & (TRDY | TMT))){
				msleep(FPGA_UART_FIFO_DELAY);
				continue;
			}
			break;
		}

		if(j >= I2C_RW_RETRY_COUNT){
			status = -EBUSY;
			return status;
		}

		status = as7535_28xb_fpga_write(FPGA_UART_IP_BUS_NUM
			, FPGA_UART_IP_ADDR, FPGA_UART_GPS_REG_TX_DATA
			, packet[i]);

		if (unlikely(status < 0))
			return status;
	}

	return 0;
}

int read_gps_packet_from_gnss(void)
{
	int status = 0;
	int i = 0;

	for(i = 0; i < I2C_RW_RETRY_COUNT; i++){
		// check FIFO empty
		status = as7535_28xb_fpga_read(FPGA_UART_IP_BUS_NUM, 
			  FPGA_UART_IP_ADDR, FPGA_UART_GPS_REG_RX_STATUS);
		if (unlikely(status < 0))
			return status;

		if(status & GPS_RX_FIFO_EMPTY)
			msleep(FPGA_UART_FIFO_DELAY);
		else	
			break;
	}

	if(i >= I2C_RW_RETRY_COUNT)
		return 0xFF;

	status = as7535_28xb_fpga_read(FPGA_UART_IP_BUS_NUM, 
		  FPGA_UART_IP_ADDR, FPGA_UART_GPS_REG_RX_DATA);

	return status;
}

static ssize_t set_tod_tx_data(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct as7535_28xb_fpga_data *data = i2c_get_clientdata(client);

	int status = 0;
	int val = 0, i = 0, j = 0;
	u8 bus = 0, addr = 0, reg = 0;

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
		status = val = as7535_28xb_fpga_read(bus, addr, reg);
		if (unlikely(status < 0))
			goto exit;

		val &= 0xF0;
		status = as7535_28xb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		val |= 0x3;
		status = as7535_28xb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		// Enable ToD_RX into FIFO
		status = val = as7535_28xb_fpga_read(bus, addr, reg);
		if (unlikely(status < 0))
			goto exit;		

		val |= 0x8;
		status = as7535_28xb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		// TOD OUTPUT Sel and Enable
		reg = FPGA_UART_TOD_OUT_SELECTION;
		val = 0x2;
		status = as7535_28xb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		reg = FPGA_UART_TOD_OUTPUT_ENABLE;
		val = 0x2;
		status = as7535_28xb_fpga_write(bus, addr, reg, val);
		if (unlikely(status < 0))
			goto exit;

		// TOD OUTPUT
		bus = FPGA_UART_IP_BUS_NUM;
		addr = FPGA_UART_IP_ADDR;

		for(i = 0; i < count-1; i++) {

			reg = FPGA_UART_TOD_REG_TX_STATUS;
			for(j = 0; j < I2C_RW_RETRY_COUNT; j++){
				status = val = as7535_28xb_fpga_read(bus, 
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
			status = as7535_28xb_fpga_write(bus, addr, reg, 
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

static ssize_t set_to_nmea(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct as7535_28xb_fpga_data *data = i2c_get_clientdata(client);

	u8 packet[] = { DLE, SET_PORT_CONFIG, PORT1, BAUT_RATE, BAUT_RATE, 
			DATA_BITS_8, PARITY_ODD, STOP_BIT_1, 0, IN_NMEA, 
			OUT_NMEA, 0, DLE, ETX };

	u32 baud_rate_sel[] = {4800, 9600, 19200, 38400, 57600, 115200};

	int status = 0;
	int i = 0;

	unsigned long baud_rate = 0;

	status = kstrtoul(buf, 0, &baud_rate);

	if (status)
		return status;

	for(i = 0; i < (sizeof(baud_rate_sel)/sizeof(baud_rate_sel[0])); i++){
		if(baud_rate_sel[i] == baud_rate)
			packet[3] = packet[4] = i+6;
	}

	mutex_lock(&data->update_lock);

	switch(attr->index){
	case NMEA_PROTOCOL:

		// GPS OUTPUT Sel
		status = fpga_gps_uart_output_sel(FPGA_TO_UART_IP0);
		if (unlikely(status < 0))
			goto exit;

		// disable GPS_RX into FIFO
		status = gps_rx_into_fifo(0);
		if (unlikely(status < 0))
			goto exit;

		// to clear FIFO
		status = uart_fifo_clear();
		if (unlikely(status < 0))
			goto exit;

		// enable GPS_RX into FIFO
		status = gps_rx_into_fifo(1);
		if (unlikely(status < 0))
			goto exit;

		// gps write to 0x62
		status = send_gps_packet_to_gnss(packet, 
				(sizeof(packet) / sizeof(packet[0])));

		if (unlikely(status < 0))
			goto exit;

		break;

	default:
		mutex_unlock(&data->update_lock);
		return -ENXIO;
	}

	status = count;
exit:
	mutex_unlock(&data->update_lock);

	return status;
}

static ssize_t set_nmea_interval_msgmsk(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct as7535_28xb_fpga_data *data = i2c_get_clientdata(client);

	u8 packet[] = { DLE, SET_NMEA_INTERVAL_MSGMSK, 
			NMEA_INTERVAL_MSGMSK_SUBID, 1, 0x3D, 0x0, 0x0, 0x0, 
			DLE, ETX };

	u8 resp_packet[NMEA_INTERVAL_MSGMSK_BYTE_NUM] = {0};

	int report_packet_length = 0;
	int status = 0;
	int i = 0;

	unsigned long nmea_output_setting = 0;
	status = kstrtoul(buf, 16, &nmea_output_setting);

	if (status)
		return status;

	packet[3] = (nmea_output_setting >> 32) & 0xFF;
	packet[4] = (nmea_output_setting >> 24) & 0xFF;
	packet[5] = (nmea_output_setting >> 16) & 0xFF;
	packet[6] = (nmea_output_setting >> 8) & 0xFF;
	packet[7] = nmea_output_setting & 0xFF;

	report_packet_length = NMEA_INTERVAL_MSGMSK_BYTE_NUM;

	mutex_lock(&data->update_lock);

	switch(attr->index){
	case NMEA_INTERVAL_MSGMSK:

		// GPS OUTPUT Sel
		status = fpga_gps_uart_output_sel(FPGA_TO_UART_IP0);
		if (unlikely(status < 0))
			goto exit;

		// disable GPS_RX into FIFO
		status = gps_rx_into_fifo(0);
		if (unlikely(status < 0))
			goto exit;

		// to clear FIFO
		status = uart_fifo_clear();
		if (unlikely(status < 0))
			goto exit;

		// enable GPS_RX into FIFO
		status = gps_rx_into_fifo(1);
		if (unlikely(status < 0))
			goto exit;

		// gps write to 0x62
		status = send_gps_packet_to_gnss(packet, 
				(sizeof(packet) / sizeof(packet[0])));

		if (unlikely(status < 0))
			goto exit;

		// read the data
		for(i = 0; i < report_packet_length; i++){
			status = read_gps_packet_from_gnss();
			if (unlikely(status < 0))
				goto exit;

			resp_packet[i] = status;
		}

		break;

	default:
		mutex_unlock(&data->update_lock);
		return -ENXIO;
	}

	status = count;
exit:
	mutex_unlock(&data->update_lock);
	return status;

}

static ssize_t get_gnss_data(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct as7535_28xb_fpga_data *data = i2c_get_clientdata(client);

	int status = 0;
	int count = 0;
	int command_packet_length = 0;
	int report_packet_length = 0;
	int i = 0;

	u8 packet[MAX_TSIP_PACKET_LENGTH] = {0};

	packet[0] = DLE; // start packet

	switch(attr->index){
	case GPS_TIME:
		packet[1] = CURRENT_TIME_REQ;
		packet[2] = DLE;
		packet[3] = ETX;
		report_packet_length = CURRENT_TIME_BYTE_NUM;
		break;

	case PORT_CONFIG:
		packet[1] = SET_PORT_CONFIG;
		packet[2] = PORT1;
		packet[3] = DLE;
		packet[4] = ETX;
		report_packet_length = PORT_COFIG_BYTE_NUM;
		break;

	case GPS_RX_FIFO:
		report_packet_length = 512;
		break;

	default:
		return -ENXIO;
	}

	if(attr->index == GPS_RX_FIFO){
		command_packet_length = 0;
	} else {
		for(i = 0; i < MAX_TSIP_PACKET_LENGTH; i++){
			if(packet[i] == DLE && packet[i+1] == ETX){
				command_packet_length = i+2;
				break;
			}
		}
	}

	mutex_lock(&data->update_lock);	

	// GPS OUTPUT Sel
	status = fpga_gps_uart_output_sel(FPGA_TO_UART_IP0);
	if (unlikely(status < 0))
		goto exit;

	// gps write to 0x62
	status = send_gps_packet_to_gnss(packet, command_packet_length);

	if (unlikely(status < 0))
		goto exit;

	// read the data
	for(i = 0; i < report_packet_length; i++){

		status = read_gps_packet_from_gnss();
		if (unlikely(status < 0))
			goto exit;

		if(attr->index == GPS_RX_FIFO && status == 0xFF){
			break;
		} else {
			buf += sprintf(buf, "%02X ", status);
			count += 3;

		}
	}

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static void as7535_28xb_fpga_add_client(struct i2c_client *client)
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

static void as7535_28xb_fpga_remove_client(struct i2c_client *client)
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
static int as7535_28xb_fpga_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adap = to_i2c_adapter(client->dev.parent);
	struct as7535_28xb_fpga_data *data;
	int ret = -ENODEV;
	const struct attribute_group *group = NULL;

	if (!i2c_check_functionality(adap, I2C_FUNC_SMBUS_BYTE))
		goto exit;

	data = kzalloc(sizeof(struct as7535_28xb_fpga_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	data->type = id->driver_data;

	/* Register sysfs hooks */
	switch (data->type) {
	case as7535_28xb_fpga:
		group = &as7535_28xb_fpga_group;
		break;
	case as7535_28xb_uartip:
		group = &as7535_28xb_uartip_group;
		break;
	default:
		break;
	}

	if (group) {
		ret = sysfs_create_group(&client->dev.kobj, group);
		if (ret)
			goto exit_free;
	}

	as7535_28xb_fpga_add_client(client);
	return 0;

exit_free:
	kfree(data);
exit:
	return ret;
}

static int as7535_28xb_fpga_remove(struct i2c_client *client)
{
	struct as7535_28xb_fpga_data *data = i2c_get_clientdata(client);
	const struct attribute_group *group = NULL;

	as7535_28xb_fpga_remove_client(client);

	/* Remove sysfs hooks */
	switch (data->type) {
	case as7535_28xb_fpga:
		group = &as7535_28xb_fpga_group;
		break;
	case as7535_28xb_uartip:
		group = &as7535_28xb_uartip_group;
		break;
	default:
		break;
	}

	if (group)
		sysfs_remove_group(&client->dev.kobj, group);

	kfree(data);

	return 0;
}

int as7535_28xb_fpga_read(int bus_num, unsigned short fpga_addr, u8 reg)
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

int as7535_28xb_fpga_write(int bus_num, unsigned short fpga_addr, u8 reg, u8 value)
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

static struct i2c_driver as7535_28xb_fpga_driver = {
	.driver         = {
		.name   = "as7535_28xb_fpga",
		.owner  = THIS_MODULE,
	},
	.probe          = as7535_28xb_fpga_probe,
	.remove         = as7535_28xb_fpga_remove,
	.id_table       = as7535_28xb_fpga_id,
};


static int __init as7535_28xb_fpga_init(void)
{
	mutex_init(&list_lock);
	return i2c_add_driver(&as7535_28xb_fpga_driver);
}

static void __exit as7535_28xb_fpga_exit(void)
{
	i2c_del_driver(&as7535_28xb_fpga_driver);
}

MODULE_AUTHOR("Alex Lai <alex_lai@edge-core.com>");
MODULE_DESCRIPTION("Accton I2C FPGA driver");
MODULE_LICENSE("GPL");

module_init(as7535_28xb_fpga_init);
module_exit(as7535_28xb_fpga_exit);
