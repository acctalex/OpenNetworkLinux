/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2016 Accton Technology Corporation.
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
#include "x86_64_accton_as5912_54x_int.h"
#include "x86_64_accton_as5912_54x_log.h"

#define PORT_BUS_INDEX(port) (port+26)

#define PORT_EEPROM_FORMAT              "/sys/bus/i2c/devices/%d-0050/eeprom"
#define MODULE_PRESENT_FORMAT		    "/sys/bus/i2c/devices/%d-00%d/module_present_%d"
#define MODULE_RXLOS_FORMAT             "/sys/bus/i2c/devices/%d-00%d/module_rx_los_%d"
#define MODULE_TXFAULT_FORMAT           "/sys/bus/i2c/devices/%d-00%d/module_tx_fault_%d"
#define MODULE_TXDISABLE_FORMAT         "/sys/bus/i2c/devices/%d-00%d/module_tx_disable_%d"
#define MODULE_RESET_FORMAT             "/sys/bus/i2c/devices/%d-00%d/module_reset_%d"
#define MODULE_LPMODE_FORMAT            "/sys/bus/i2c/devices/%d-00%d/module_lpmode_%d"
#define MODULE_PRESENT_ALL_ATTR	        "/sys/bus/i2c/devices/%d-00%d/module_present_all"
#define MODULE_RXLOS_ALL_ATTR_CPLD1	    "/sys/bus/i2c/devices/4-0060/module_rx_los_all"
#define MODULE_RXLOS_ALL_ATTR_CPLD2	    "/sys/bus/i2c/devices/5-0062/module_rx_los_all"

#define PORT_MAX_NUM 54
#define VALIDATE_SFP(_port) \
    do { \
        if (_port < 0 || _port >= 48) \
            return ONLP_STATUS_E_UNSUPPORTED; \
    } while(0)

#define VALIDATE_QSFP(_port) \
    do { \
        if (_port < 48 || _port >= PORT_MAX_NUM) \
            return ONLP_STATUS_E_UNSUPPORTED; \
    } while(0)

/************************************************************
 *
 * SFPI Entry Points
 *
 ***********************************************************/
int
onlp_sfpi_init(void)
{
    /* Called at initialization time */    
    return ONLP_STATUS_OK;
}

int
onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
    /*
     * Ports {0, 54}
     */
    int p;

    for(p = 0; p < 54; p++) {
        AIM_BITMAP_SET(bmap, p);
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_is_present(int port)
{
    /*
     * Return 1 if present.
     * Return 0 if not present.
     * Return < 0 if error.
     */
    int present;
    int bus, addr;

    addr = (port < 24) ? 60 : 62;
    bus  = (addr == 60) ? 4 : 5;
    
	if (onlp_file_read_int(&present, MODULE_PRESENT_FORMAT, bus, addr, (port+1)) < 0) {
        AIM_LOG_ERROR("Unable to read present status from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }
    
    return present;
}

int
onlp_sfpi_presence_bitmap_get(onlp_sfp_bitmap_t* dst)
{
    uint32_t bytes[7], *ptr = NULL;
    FILE* fp;
    int addr;

    ptr = bytes;

    for (addr = 60; addr <= 62; addr+=2) {
        /* Read present status of port 0~53 */
        int count = 0;
        char file[64] = {0};
        int bus = (addr == 60) ? 4 : 5;
        
        sprintf(file, MODULE_PRESENT_ALL_ATTR, bus, addr);
        fp = fopen(file, "r");
        if(fp == NULL) {
            AIM_LOG_ERROR("Unable to open the module_present_all device file of CPLD(0x%d).", addr);
            return ONLP_STATUS_E_INTERNAL;
        }

        if (addr == 60) { /* CPLD1 */
            count = fscanf(fp, "%x %x %x", ptr+0, ptr+1, ptr+2);
            fclose(fp);
            if(count != 3) {
                /* Likely a CPLD read timeout. */
                AIM_LOG_ERROR("Unable to read all fields the module_present_all device file of CPLD(0x%d).", addr);
                return ONLP_STATUS_E_INTERNAL;
            }
        }
        else { /* CPLD2 */
            count = fscanf(fp, "%x %x %x %x", ptr+0, ptr+1, ptr+2, ptr+3);
            fclose(fp);
            if(count != 4) {
                /* Likely a CPLD read timeout. */
                AIM_LOG_ERROR("Unable to read all fields the module_present_all device file of CPLD(0x%d).", addr);
                return ONLP_STATUS_E_INTERNAL;
            }
        }

        ptr += count;
    }

    /* Mask out non-existant QSFP ports */
    bytes[6] &= 0x3F;

    /* Convert to 64 bit integer in port order */
    int i = 0;
    uint64_t presence_all = 0 ;
    for(i = AIM_ARRAYSIZE(bytes)-1; i >= 0; i--) {
        presence_all <<= 8;
        presence_all |= bytes[i];
    }

    /* Populate bitmap */
    for(i = 0; presence_all; i++) {
        AIM_BITMAP_MOD(dst, i, (presence_all & 1));
        presence_all >>= 1;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_rx_los_bitmap_get(onlp_sfp_bitmap_t* dst)
{
    uint32_t bytes[6];
    uint32_t *ptr = bytes;
    FILE* fp;

    /* Read present status of port 0~23 */
    int addr, i = 0;

    for (addr = 60; addr <= 62; addr+=2) {
        if (addr == 60) {
            fp = fopen(MODULE_RXLOS_ALL_ATTR_CPLD1, "r");
        }
        else {
            fp = fopen(MODULE_RXLOS_ALL_ATTR_CPLD2, "r");
        }

        if(fp == NULL) {
            AIM_LOG_ERROR("Unable to open the module_rx_los_all device file of CPLD(0x%d)", addr);
            return ONLP_STATUS_E_INTERNAL;
        }

        int count = fscanf(fp, "%x %x %x", ptr+0, ptr+1, ptr+2);
        fclose(fp);
        if(count != 3) {
            /* Likely a CPLD read timeout. */
            AIM_LOG_ERROR("Unable to read all fields from the module_rx_los_all device file of CPLD(0x%d)", addr);
            return ONLP_STATUS_E_INTERNAL;
        }

        ptr += count;
    }

    /* Convert to 64 bit integer in port order */
    i = 0;
    uint64_t rx_los_all = 0 ;
    for(i = AIM_ARRAYSIZE(bytes)-1; i >= 0; i--) {
        rx_los_all <<= 8;
        rx_los_all |= bytes[i];
    }

    /* Populate bitmap */
    for(i = 0; rx_los_all; i++) {
        AIM_BITMAP_MOD(dst, i, (rx_los_all & 1));
        rx_los_all >>= 1;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_eeprom_read(int port, uint8_t data[256])
{
    /*
     * Read the SFP eeprom into data[]
     *
     * Return MISSING if SFP is missing.
     * Return OK if eeprom is read
     */
    int size = 0;
    memset(data, 0, 256);

	if(onlp_file_read(data, 256, &size, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port)) != ONLP_STATUS_OK) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (size != 256) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d), size is different!\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_dom_read(int port, uint8_t data[256])
{
    FILE* fp;
    char file[64] = {0};
    
    sprintf(file, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port));
    fp = fopen(file, "r");
    if(fp == NULL) {
        AIM_LOG_ERROR("Unable to open the eeprom device file of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (fseek(fp, 256, SEEK_CUR) != 0) {
        fclose(fp);
        AIM_LOG_ERROR("Unable to set the file position indicator of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    int ret = fread(data, 1, 256, fp);
    fclose(fp);
    if (ret != 256) {
        AIM_LOG_ERROR("Unable to read the module_eeprom device file of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_dev_readb(int port, uint8_t devaddr, uint8_t addr)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_readb(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_writeb(int port, uint8_t devaddr, uint8_t addr, uint8_t value)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_writeb(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_readw(int port, uint8_t devaddr, uint8_t addr)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_readw(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_writew(int port, uint8_t devaddr, uint8_t addr, uint16_t value)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_writew(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
    int rv;

    if (port < 0 || port >= PORT_MAX_NUM) {
        return ONLP_STATUS_E_UNSUPPORTED;
    }

    int addr = (port < 24) ? 60 : 62;
    int bus  = (addr == 60) ? 4 : 5;

    switch(control)
        {
        case ONLP_SFP_CONTROL_TX_DISABLE:
            {
                VALIDATE_SFP(port);

                if (onlp_file_write_int(value, MODULE_TXDISABLE_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to set tx_disable status to port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        case ONLP_SFP_CONTROL_RESET:
            {
                VALIDATE_QSFP(port);

                if (onlp_file_write_int(value, MODULE_RESET_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to set reset status to port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        case ONLP_SFP_CONTROL_LP_MODE:
            {
                VALIDATE_QSFP(port);

                if (onlp_file_write_int(value, MODULE_LPMODE_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to set lpmode status to port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        default:
            rv = ONLP_STATUS_E_UNSUPPORTED;
            break;
        }

    return rv;
}

int
onlp_sfpi_control_get(int port, onlp_sfp_control_t control, int* value)
{
    int rv;

    if (port < 0 || port >= PORT_MAX_NUM) {
        return ONLP_STATUS_E_UNSUPPORTED;
    }

    int addr = (port < 24) ? 60 : 62;
    int bus  = (addr == 60) ? 4 : 5;

    switch(control)
        {
        case ONLP_SFP_CONTROL_RX_LOS:
            {
                VALIDATE_SFP(port);

            	if (onlp_file_read_int(value, MODULE_RXLOS_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to read rx_loss status from port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        case ONLP_SFP_CONTROL_TX_FAULT:
            {
                VALIDATE_SFP(port);

            	if (onlp_file_read_int(value, MODULE_TXFAULT_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to read tx_fault status from port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        case ONLP_SFP_CONTROL_TX_DISABLE:
            {
                VALIDATE_SFP(port);

            	if (onlp_file_read_int(value, MODULE_TXDISABLE_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to read tx_disabled status from port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        case ONLP_SFP_CONTROL_RESET:
            {
                VALIDATE_QSFP(port);

                if (onlp_file_read_int(value, MODULE_RESET_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to read reset status from port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        case ONLP_SFP_CONTROL_LP_MODE:
            {
                VALIDATE_QSFP(port);

                if (onlp_file_read_int(value, MODULE_LPMODE_FORMAT, bus, addr, (port+1)) < 0) {
                    AIM_LOG_ERROR("Unable to read lpmode status from port(%d)\r\n", port);
                    rv = ONLP_STATUS_E_INTERNAL;
                }
                else {
                    rv = ONLP_STATUS_OK;
                }
                break;
            }

        default:
            rv = ONLP_STATUS_E_UNSUPPORTED;
        }

    return rv;
}

int
onlp_sfpi_denit(void)
{
    return ONLP_STATUS_OK;
}

