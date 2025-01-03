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
#include "x86_64_accton_as9736_64d_int.h"
#include "x86_64_accton_as9736_64d_log.h"
#include "platform_lib.h"

#define UDB_PORT_EEPROM_FORMAT \
	"/sys/bus/platform/devices/pcie_udb_fpga_device.%d/eeprom"

#define LDB_PORT_EEPROM_FORMAT \
	"/sys/bus/platform/devices/pcie_ldb_fpga_device.%d/eeprom"

#define MODULE_PRESENT_FORMAT \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_present_%d"
#define MODULE_RXLOS_FORMAT \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_rx_los_%d"
#define MODULE_TXFAULT_FORMAT \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_tx_fault_%d"
#define MODULE_TXDISABLE_FORMAT \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_tx_disable_%d"
#define MODULE_RESET_FORMAT \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_reset_%d"
#define MODULE_LPMODE_FORMAT \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_lp_mode_%d"
#define MODULE_PRESENT_ALL_ATTR \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_present_all"
#define MODULE_RXLOS_ALL_ATTR \
	"/sys/bus/platform/devices/as9736_64d_fpga/module_rx_los_all"

#if 0
int sfp_map_bus[] = {17, 18, 19, 20, 21, 22, 23, 24,
		    25, 26, 27, 28, 29, 30, 31, 32,
		    33, 34, 35, 36, 37, 38, 39, 40,
		    41, 42, 43, 44, 45, 46, 47, 48,
		    49, 50};
#endif

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

#if 0
int onlp_sfpi_map_bus_index(int port)
{
	if (port < 0 || port >= 34)
		return ONLP_STATUS_E_INTERNAL;
	return sfp_map_bus[port];
}
#endif

int onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
	/*
	 * Ports {0, 65}
	 */
	int p;

	for (p = 0; p < 66; p++)
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

	if (port < 0 || port > 66)
		return ONLP_STATUS_E_INTERNAL;

	if (onlp_file_read_int(&present, MODULE_PRESENT_FORMAT, (port+1)) < 0) {
		AIM_LOG_ERROR("Unable to read present status from port(%d)\r\n"
				, port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return present;
}

int onlp_sfpi_presence_bitmap_get(onlp_sfp_bitmap_t* dst)
{
	int i = 1;
	int present = 0;

	for (i = 0; i <= CHASSIS_QSFP_COUNT + CHASSIS_SFP_COUNT-1; i++) {        
		present = onlp_sfpi_is_present(i);
		AIM_BITMAP_MOD(dst, i, (1 == present) ? 1 : 0);
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_rx_los_bitmap_get(onlp_sfp_bitmap_t* dst)
{
#if 0
	uint32_t bytes[9];
	uint32_t *ptr = bytes;
	FILE* fp;

	int i = 0;

	AIM_BITMAP_CLR_ALL(dst);

	fp = fopen(MODULE_RXLOS_ALL_ATTR, "r");
	if (fp == NULL) {
		AIM_LOG_ERROR("Unable to open the module_rx_los_all device file.");
		return ONLP_STATUS_E_INTERNAL;
	}

	int count = fscanf(fp, "%x %x %x %x %x %x %x %x %x", ptr+3, ptr+2,
				ptr+1, ptr+0, ptr+7, ptr+6, ptr+5, ptr+4,
				ptr+8);
	fclose(fp);
	if (count != 9) {
		/* Likely a read timeout. */
		AIM_LOG_ERROR("Unable to read all fields from the module_rx_los_all device file.");
		return ONLP_STATUS_E_INTERNAL;
	}

	uint64_t rx_los_all = 0;
	
	for (i = AIM_ARRAYSIZE(bytes)-1; i >= 0; i--) {
		rx_los_all <<= 8;
		rx_los_all |= bytes[i];
	}

	/* Populate bitmap */
	for (i = 0; rx_los_all; i++) {
		AIM_BITMAP_MOD(dst, i, (rx_los_all & 1));
		rx_los_all >>= 1;
	}

	return ONLP_STATUS_OK;
#endif
	//=========================================

	int i = 0;
	int rx_loss = 0;

	AIM_BITMAP_CLR_ALL(dst);

	for(i = 0; i < CHASSIS_QSFP_COUNT+CHASSIS_SFP_COUNT; i++) {

		if (i < CHASSIS_QSFP_COUNT) {
			AIM_BITMAP_MOD(dst, i, 0);
		} else {
			if (onlp_file_read_int(&rx_loss, MODULE_RXLOS_FORMAT, 
			(i+1)) < 0) {
				AIM_LOG_ERROR("Unable to read rxloss status from port(%d)\r\n"
					, i+1);
				return ONLP_STATUS_E_INTERNAL;
			}

			AIM_BITMAP_MOD(dst, i, (1 == rx_loss) ? 1 : 0);
		}		
	}
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
	if (port < 0 || port > 65)
		return ONLP_STATUS_E_INTERNAL;
	memset(data, 0, 256);

	if (port <= 31) {
		if (onlp_file_read(data, 256, &size, UDB_PORT_EEPROM_FORMAT,
					port) != ONLP_STATUS_OK) {
			AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
			return ONLP_STATUS_E_INTERNAL;
		}
	} else {
		if (onlp_file_read(data, 256, &size, LDB_PORT_EEPROM_FORMAT,
					port-32) != ONLP_STATUS_OK) {
			AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
			return ONLP_STATUS_E_INTERNAL;
		}
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
#if 0
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
#endif
	return ONLP_STATUS_OK;
}

int onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
	int rv;
	int addr = 0;
	int bus  = 10;

	switch(control) {
	case ONLP_SFP_CONTROL_TX_DISABLE:
		if (port == 32 || port == 33) {
			addr = 62;
			if (onlp_file_write_int(value, MODULE_TXDISABLE_FORMAT,
						bus, addr, (port + 1)) < 0) {
				AIM_LOG_ERROR("Unable to set tx_disable status to port(%d)\r\n", 
					      port);
				rv = ONLP_STATUS_E_INTERNAL;
			} else {
				rv = ONLP_STATUS_OK;
			}
		} else {
			rv = ONLP_STATUS_E_UNSUPPORTED;
		}
		break;

	case ONLP_SFP_CONTROL_RESET:
		if (port >= 0 && port < 16) {
			addr = 61;
		} else if(port >= 16 && port < 32) {
			addr = 62;
		} else {
			rv = ONLP_STATUS_E_UNSUPPORTED;
			break;
		}

		if (onlp_file_write_int(value, MODULE_RESET_FORMAT,
					bus, addr, (port + 1)) < 0) {
			AIM_LOG_ERROR("Unable to set reset status to port(%d)\r\n", 
				      port);
			rv = ONLP_STATUS_E_INTERNAL;
		} else {
			rv = ONLP_STATUS_OK;
		}
		break;

	case ONLP_SFP_CONTROL_LP_MODE:
		if (port >= 64) {
			rv = ONLP_STATUS_E_UNSUPPORTED;
			break;
		}

		if (onlp_file_write_int(value, MODULE_LPMODE_FORMAT,
					(port + 1)) < 0) {
			AIM_LOG_ERROR("Unable to set lp mode to port(%d)\r\n", 
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

	switch (control) {
	case ONLP_SFP_CONTROL_RX_LOS:
		if (port >= 64) {
			if (onlp_file_read_int(value, MODULE_RXLOS_FORMAT, 
					       (port+1)) < 0) {
				AIM_LOG_ERROR("Unable to read rx_loss status from port(%d)\r\n",
					      port);
				rv = ONLP_STATUS_E_INTERNAL;
			} else {
				rv = ONLP_STATUS_OK;
			}
		} else {
			rv = ONLP_STATUS_E_UNSUPPORTED;
		}
		break;

	case ONLP_SFP_CONTROL_TX_FAULT:
		if (port >= 64) {
			if (onlp_file_read_int(value, MODULE_TXFAULT_FORMAT,
					       (port+1)) < 0) {
				AIM_LOG_ERROR("Unable to read tx_fault status from port(%d)\r\n",
					      port);
				rv = ONLP_STATUS_E_INTERNAL;
			} else {
				rv = ONLP_STATUS_OK;
			}
		} else {
			rv = ONLP_STATUS_E_UNSUPPORTED;
		}
		break;

	case ONLP_SFP_CONTROL_TX_DISABLE:
		if (port >= 64) {
			if (onlp_file_read_int(value, MODULE_TXDISABLE_FORMAT,
					       (port+1)) < 0) {
				AIM_LOG_ERROR("Unable to read tx_disabled status from port(%d)\r\n", 
					      port);
				rv = ONLP_STATUS_E_INTERNAL;
			} else {
				rv = ONLP_STATUS_OK;
			}
		} else {
			rv = ONLP_STATUS_E_UNSUPPORTED;
		}
		break;

	case ONLP_SFP_CONTROL_RESET:
		if (port >= 66) {
			rv = ONLP_STATUS_E_UNSUPPORTED;
			break;
		}

		if (onlp_file_read_int(value, MODULE_RESET_FORMAT,
					(port + 1)) < 0) {
			AIM_LOG_ERROR("Unable to get reset status to port(%d)\r\n", 
				      port);
			rv = ONLP_STATUS_E_INTERNAL;
		} else {
			rv = ONLP_STATUS_OK;
		}
		break;	

	case ONLP_SFP_CONTROL_LP_MODE:
		if (port >= 64) {
			rv = ONLP_STATUS_E_UNSUPPORTED;
			break;
		}

		if (onlp_file_read_int(value, MODULE_LPMODE_FORMAT,
					(port + 1)) < 0) {
			AIM_LOG_ERROR("Unable to get lp mode to port(%d)\r\n", 
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
