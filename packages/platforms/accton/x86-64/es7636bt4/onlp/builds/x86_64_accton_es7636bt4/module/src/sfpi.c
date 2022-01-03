/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2013 Accton Technology Corporation.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/
#include <onlp/platformi/sfpi.h>
#include <onlplib/i2c.h>
#include <onlplib/file.h>
#include "x86_64_accton_es7636bt4_int.h"
#include "x86_64_accton_es7636bt4_log.h"


#define PORT_EEPROM_FORMAT        "/sys/bus/i2c/devices/%d-0050/eeprom"
#define MODULE_PRESENT_FORMAT \
	"/sys/bus/i2c/devices/%d-00%d/module_present_%d"
#define MODULE_RESET_FORMAT \
	"/sys/bus/i2c/devices/%d-00%d/module_reset_%d"
#define MODULE_PRESENT_ALL_ATTR \
	"/sys/bus/i2c/devices/%d-00%d/module_present_all"

int sfp_map_bus[] = {25, 26, 27, 28, 29, 30, 31, 32,
		    33, 34, 35, 36, 37, 38, 39, 40,
		    41, 42, 43, 44, 45, 46, 47, 48,
		    49, 50, 51, 52, 57, 58, 59, 60,
		    61, 62, 63, 64};

/************************************************************
 *
 * SFPI Entry Points
 *
 ***********************************************************/

int onlp_sfpi_init(void)
{
	/* Called at initialization time */
	return ONLP_STATUS_OK;
}

int onlp_sfpi_map_bus_index(int port)
{
	if (port < 0 || port >= 36)
		return ONLP_STATUS_E_INTERNAL;
	return sfp_map_bus[port];
}

int onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
	/*
	 * Ports {0, 36}
	 */
	int p;

	for (p = 0; p < 36; p++)
		AIM_BITMAP_SET(bmap, p);

	return ONLP_STATUS_OK;
}

int onlp_sfpi_is_present(int port)
{
	/*
	 * Return 1 if present.
	 * Return 0 if not present.
	 * Return < 0 if error.
	 */
	int present;
	int bus, addr;

	if (port < 0 || port > 36)
		return ONLP_STATUS_E_INTERNAL;

	addr = 60;
	bus  = 10;

	if (onlp_file_read_int(&present, MODULE_PRESENT_FORMAT, bus, 
				addr, (port+1)) < 0) {
		AIM_LOG_ERROR("Unable to read present status from port(%d)\r\n"
			      , port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return present;
}

int onlp_sfpi_presence_bitmap_get(onlp_sfp_bitmap_t* dst)
{
	uint32_t bytes[5], *ptr = NULL;
	FILE* fp;
	int addr = 60;
	int bus = 10;
	char file[64] = {0};
	int count;

	ptr = bytes;
	sprintf(file, MODULE_PRESENT_ALL_ATTR, bus, addr);
	fp = fopen(file, "r");
	if (fp == NULL) {
		AIM_LOG_ERROR("Unable to open the module_present_all device file of CPLD1.");
		return ONLP_STATUS_E_INTERNAL;
	}

	count = fscanf(fp, "%x %x %x %x %x", ptr+0, ptr+1, ptr+2, ptr+3, 
			ptr+4);
	fclose(fp);
	if (count != 5) {
		/* Likely a CPLD read timeout. */
		AIM_LOG_ERROR("Unable to read all fields the module_present_all device file of CPLD1.");
		return ONLP_STATUS_E_INTERNAL;
	}

	/* Convert to 64 bit integer in port order */
	uint64_t presence_all = 0 ;
	int i = 0;
	for (i = AIM_ARRAYSIZE(bytes)-1; i >= 0; i--) {
		presence_all <<= 8;
		presence_all |= bytes[i];
	}

	/* Populate bitmap */
	for (i = 0; presence_all; i++) {
		AIM_BITMAP_MOD(dst, i, (presence_all & 1));
		presence_all >>= 1;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_rx_los_bitmap_get(onlp_sfp_bitmap_t* dst)
{
	AIM_BITMAP_CLR_ALL(dst);
	return ONLP_STATUS_OK;
}

int onlp_sfpi_eeprom_read(int port, uint8_t data[256])
{
	/*
	 * Read the SFP eeprom into data[]
	 *
	 * Return MISSING if SFP is missing.
	 * Return OK if eeprom is read
	 */
	int size = 0;
	if (port < 0 || port > 36)
		return ONLP_STATUS_E_INTERNAL;
	memset(data, 0, 256);

	if (onlp_file_read(data, 256, &size, PORT_EEPROM_FORMAT,
			  onlp_sfpi_map_bus_index(port)) != ONLP_STATUS_OK) {
		AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
		return ONLP_STATUS_E_INTERNAL;
	}

	if (size != 256) {
		AIM_LOG_ERROR("Unable to read eeprom from port(%d), size is different!\r\n", 
			      port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_dom_read(int port, uint8_t data[256])
{
	FILE* fp;
	char file[64] = {0};

	sprintf(file, PORT_EEPROM_FORMAT, onlp_sfpi_map_bus_index(port));
	fp = fopen(file, "r");
	if (fp == NULL) {
		AIM_LOG_ERROR("Unable to open the eeprom device file of port(%d)",
			      port);
		return ONLP_STATUS_E_INTERNAL;
	}

	if (fseek(fp, 256, SEEK_CUR) != 0) {
		fclose(fp);
		AIM_LOG_ERROR("Unable to set the file position indicator of port(%d)", 
			      port);
		return ONLP_STATUS_E_INTERNAL;
	}

	int ret = fread(data, 1, 256, fp);
	fclose(fp);
	if (ret != 256) {
		AIM_LOG_ERROR("Unable to read the module_eeprom device file of port(%d)", 
			      port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_dev_readb(int port, uint8_t devaddr, uint8_t addr)
{
	int bus = onlp_sfpi_map_bus_index(port);
	return onlp_i2c_readb(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int onlp_sfpi_dev_writeb(int port, uint8_t devaddr, uint8_t addr, 
			 uint8_t value)
{
	int bus = onlp_sfpi_map_bus_index(port);
	return onlp_i2c_writeb(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int onlp_sfpi_dev_readw(int port, uint8_t devaddr, uint8_t addr)
{
	int bus = onlp_sfpi_map_bus_index(port);
	return onlp_i2c_readw(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int onlp_sfpi_dev_writew(int port, uint8_t devaddr, uint8_t addr, 
			 uint16_t value)
{
	int bus = onlp_sfpi_map_bus_index(port);
	return onlp_i2c_writew(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
	int rv;
	int addr = 60;
	int bus  = 10;

	switch(control) {
	case ONLP_SFP_CONTROL_RESET:
		if (onlp_file_write_int(value, MODULE_RESET_FORMAT,
					       bus, addr, (port + 1)) < 0) {
			AIM_LOG_ERROR("Unable to set reset status from port(%d)\r\n", 
					      port);
			rv = ONLP_STATUS_E_INTERNAL;
		} else {
			rv = ONLP_STATUS_OK;
		}
		break;

	default:
		rv = ONLP_STATUS_E_UNSUPPORTED;
		break;
	}

	return rv;
}

int onlp_sfpi_control_get(int port, onlp_sfp_control_t control, int* value)
{
	int rv;
	int addr = 60;
	int bus  = 10;

	switch (control) {
	case ONLP_SFP_CONTROL_RESET:
		if (onlp_file_read_int(value, MODULE_RESET_FORMAT,
					       bus, addr, (port + 1)) < 0) {
			AIM_LOG_ERROR("Unable to read reset status from port(%d)\r\n", 
					      port);
			rv = ONLP_STATUS_E_INTERNAL;
		} else {
			rv = ONLP_STATUS_OK;
		}
		break;

	default:
		rv = ONLP_STATUS_E_UNSUPPORTED;
	}

	return rv;
}

int onlp_sfpi_denit(void)
{
	return ONLP_STATUS_OK;
}
