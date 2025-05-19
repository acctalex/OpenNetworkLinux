/*
 * Copyright (C)  Willy Liu <willy_liu@accton.com>
 *
 * This module supports the accton fpga via pcie that read/write reg
 * mechanism to get OSFP/SFP status ...etc.
 * This includes the:
 *     Accton as9947_72xkb FPGA
 *
 * Copyright (C) 2017 Finisar Corp.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c-mux.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/time64.h>

#define __STDC_WANT_LIB_EXT1__ 1
#include <linux/string.h>
#include <linux/platform_data/i2c-ocores.h>

/***********************************************
 *       variable define
 * *********************************************/
#define DRVNAME                        "as9947_72xkb_fpga"
#define OCORES_I2C_DRVNAME             "ocores-as9947"

#define PORT_NUM                       76 /* 48 OSFPs + 24 QSFPDDs + 4 SFPs*/
/*
 * PCIE BAR address
 */
#define BAR0_NUM                       0
#define BAR4_NUM                       4
#define BAR5_NUM                       5
/* need checking*/
#define REGION_LEN                     0xFF
#define FPGA_PCI_VENDOR_ID             0x1172
#define FPGA_PCI_DEVICE_ID             0xe001
#define FPGA_PCIE_START_OFFSET         0x0000
#define FPGA_MAJOR_VER_REG             0x01
#define FPGA_MINOR_VER_REG             0x02
#define SPI_BUSY_MASK_CPLD1            0x01
#define SPI_BUSY_MASK_CPLD2            0x02
#define FPGA_BOARD_INFO_REG            (FPGA_PCIE_START_OFFSET + 0x00)
/* CPLD */
#define CPLD_PCIE_START_OFFSET         0x2000
/* MB CPLD0, ASYNC_ Region 4, 0: pcie to MB_CPLD0 */
#define QSFP_DD_P7_P0_RESET_REG        (CPLD_PCIE_START_OFFSET + 0x60)
#define QSFP_DD_P11_P8_RESET_REG       (CPLD_PCIE_START_OFFSET + 0x61)
#define QSFP_DD_P7_P0_LPMODE_REG       (CPLD_PCIE_START_OFFSET + 0x64)
#define QSFP_DD_P11_P8_LPMODE_REG      (CPLD_PCIE_START_OFFSET + 0x65)
#define QSFP_DD_P7_P0_PRESENT_REG      (CPLD_PCIE_START_OFFSET + 0x68)
#define QSFP_DD_P11_P8_PRESENT_REG     (CPLD_PCIE_START_OFFSET + 0x69)
/* MB CPLD1, ASYNC_ Region 4, 1: pcie to MB_CPLD1 */
#define QSFP_DD_P19_P12_RESET_REG      (CPLD_PCIE_START_OFFSET + 0x60)
#define QSFP_DD_P23_P20_RESET_REG      (CPLD_PCIE_START_OFFSET + 0x61)
#define QSFP_DD_P19_P12_LPMODE_REG     (CPLD_PCIE_START_OFFSET + 0x64)
#define QSFP_DD_P23_P20_LPMODE_REG     (CPLD_PCIE_START_OFFSET + 0x65)
#define QSFP_DD_P19_P12_PRESENT_REG    (CPLD_PCIE_START_OFFSET + 0x68)
#define QSFP_DD_P23_P20_PRESENT_REG    (CPLD_PCIE_START_OFFSET + 0x69)
/* MEZZ CPLD0, ASYNC_ Region 5, 2'b00: pcie to Mezz_CPLD0 */
#define QSFP28_P7_P0_RESET_REG         (CPLD_PCIE_START_OFFSET + 0x80)
#define QSFP28_P15_P8_RESET_REG        (CPLD_PCIE_START_OFFSET + 0x81)
#define QSFP28_P7_P0_LPMODE_REG        (CPLD_PCIE_START_OFFSET + 0x88)
#define QSFP28_P15_P8_LPMODE_REG       (CPLD_PCIE_START_OFFSET + 0x89)
#define QSFP28_P7_P0_PRESENT_REG       (CPLD_PCIE_START_OFFSET + 0x90)
#define QSFP28_P15_P8_PRESENT_REG      (CPLD_PCIE_START_OFFSET + 0x91)
/* MEZZ CPLD1, ASYNC_ Region 5, 2'b01: pcie to Mezz_CPLD1 */
#define QSFP28_P39_P32_RESET_REG       (CPLD_PCIE_START_OFFSET + 0x80)
#define QSFP28_P47_P40_RESET_REG       (CPLD_PCIE_START_OFFSET + 0x81)
#define QSFP28_P39_P32_LPMODE_REG      (CPLD_PCIE_START_OFFSET + 0x88)
#define QSFP28_P47_P40_LPMODE_REG      (CPLD_PCIE_START_OFFSET + 0x89)
#define QSFP28_P39_P32_PRESENT_REG     (CPLD_PCIE_START_OFFSET + 0x90)
#define QSFP28_P47_P40_PRESENT_REG     (CPLD_PCIE_START_OFFSET + 0x91)
#define SFP_P3_P0_PRESENT_REG          (CPLD_PCIE_START_OFFSET + 0x92)
#define SFP_P3_P0_TXDIS_REG            (CPLD_PCIE_START_OFFSET + 0xA8)
#define SFP_P3_P0_RXLOS_REG            (CPLD_PCIE_START_OFFSET + 0xA9)
#define SFP_P3_P0_TXFAULT_REG          (CPLD_PCIE_START_OFFSET + 0xB2)
/* MEZZ CPLD2, ASYNC_ Region 5, 2'b10: pcie to Mezz_CPLD2 */
#define QSFP28_P23_P16_RESET_REG       (CPLD_PCIE_START_OFFSET + 0x80)
#define QSFP28_P31_P24_RESET_REG       (CPLD_PCIE_START_OFFSET + 0x81)
#define QSFP28_P23_P16_LPMODE_REG      (CPLD_PCIE_START_OFFSET + 0x88)
#define QSFP28_P31_P24_LPMODE_REG      (CPLD_PCIE_START_OFFSET + 0x89)
#define QSFP28_P23_P16_PRESENT_REG     (CPLD_PCIE_START_OFFSET + 0x90)
#define QSFP28_P31_P24_PRESENT_REG     (CPLD_PCIE_START_OFFSET + 0x91)

#define TRANSCEIVER_PRESENT_ATTR_ID(index)    MODULE_PRESENT_##index
#define TRANSCEIVER_LPMODE_ATTR_ID(index)     MODULE_LPMODE_##index
#define TRANSCEIVER_RESET_ATTR_ID(index)      MODULE_RESET_##index
#define TRANSCEIVER_TX_DISABLE_ATTR_ID(index) MODULE_TX_DISABLE_##index
#define TRANSCEIVER_TX_FAULT_ATTR_ID(index)   MODULE_TX_FAULT_##index
#define TRANSCEIVER_RX_LOS_ATTR_ID(index)     MODULE_RX_LOS_##index
/***********************************************
 *       macro define
 * *********************************************/
#define pcie_err(fmt, args...) \
        printk(KERN_ERR "["DRVNAME"]: " fmt " ", ##args)

#define pcie_info(fmt, args...) \
        printk(KERN_ERR "["DRVNAME"]: " fmt " ", ##args)


#define LOCK(lock)      \
do {                                                \
    spin_lock(lock);                                \
} while (0)

#define UNLOCK(lock)    \
do {                                                \
    spin_unlock(lock);                              \
} while (0)


/***********************************************
 *       structure & variable declare
 * *********************************************/
typedef struct pci_fpga_device_s {
    void  __iomem *data_base_addr0;
    void  __iomem *data_base_addr4;
    void  __iomem *data_base_addr5;
    resource_size_t data_region4;
    resource_size_t data_region5;
    struct pci_dev  *pci_dev;
    struct platform_device *fpga_i2c[PORT_NUM];
} pci_fpga_device_t;

/*fpga port status*/
struct as9947_72xkb_fpga_data {
    u8                  cpld_reg[2];
    unsigned long       last_updated;    /* In jiffies */
    pci_fpga_device_t   pci_fpga_dev;
};

static struct platform_device *pdev = NULL;
extern spinlock_t cpld_access_lock;
extern int wait_spi(u32 mask, unsigned long timeout);
extern void __iomem *spi_busy_reg;
extern void __iomem *async_reg;
/***********************************************
 *       enum define
 * *********************************************/
enum fpga_sysfs_attributes {
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
    TRANSCEIVER_PRESENT_ATTR_ID(37),
    TRANSCEIVER_PRESENT_ATTR_ID(38),
    TRANSCEIVER_PRESENT_ATTR_ID(39),
    TRANSCEIVER_PRESENT_ATTR_ID(40),
    TRANSCEIVER_PRESENT_ATTR_ID(41),
    TRANSCEIVER_PRESENT_ATTR_ID(42),
    TRANSCEIVER_PRESENT_ATTR_ID(43),
    TRANSCEIVER_PRESENT_ATTR_ID(44),
    TRANSCEIVER_PRESENT_ATTR_ID(45),
    TRANSCEIVER_PRESENT_ATTR_ID(46),
    TRANSCEIVER_PRESENT_ATTR_ID(47),
    TRANSCEIVER_PRESENT_ATTR_ID(48),
    TRANSCEIVER_PRESENT_ATTR_ID(49),
    TRANSCEIVER_PRESENT_ATTR_ID(50),
    TRANSCEIVER_PRESENT_ATTR_ID(51),
    TRANSCEIVER_PRESENT_ATTR_ID(52),
    TRANSCEIVER_PRESENT_ATTR_ID(53),
    TRANSCEIVER_PRESENT_ATTR_ID(54),
    TRANSCEIVER_PRESENT_ATTR_ID(55),
    TRANSCEIVER_PRESENT_ATTR_ID(56),
    TRANSCEIVER_PRESENT_ATTR_ID(57),
    TRANSCEIVER_PRESENT_ATTR_ID(58),
    TRANSCEIVER_PRESENT_ATTR_ID(59),
    TRANSCEIVER_PRESENT_ATTR_ID(60),
    TRANSCEIVER_PRESENT_ATTR_ID(61),
    TRANSCEIVER_PRESENT_ATTR_ID(62),
    TRANSCEIVER_PRESENT_ATTR_ID(63),
    TRANSCEIVER_PRESENT_ATTR_ID(64),
    TRANSCEIVER_PRESENT_ATTR_ID(65),
    TRANSCEIVER_PRESENT_ATTR_ID(66),
    TRANSCEIVER_PRESENT_ATTR_ID(67),
    TRANSCEIVER_PRESENT_ATTR_ID(68),
    TRANSCEIVER_PRESENT_ATTR_ID(69),
    TRANSCEIVER_PRESENT_ATTR_ID(70),
    TRANSCEIVER_PRESENT_ATTR_ID(71),
    TRANSCEIVER_PRESENT_ATTR_ID(72),
    TRANSCEIVER_PRESENT_ATTR_ID(73),
    TRANSCEIVER_PRESENT_ATTR_ID(74),
    TRANSCEIVER_PRESENT_ATTR_ID(75),
    TRANSCEIVER_PRESENT_ATTR_ID(76),
    /*Reset*/
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
    TRANSCEIVER_RESET_ATTR_ID(37),
    TRANSCEIVER_RESET_ATTR_ID(38),
    TRANSCEIVER_RESET_ATTR_ID(39),
    TRANSCEIVER_RESET_ATTR_ID(40),
    TRANSCEIVER_RESET_ATTR_ID(41),
    TRANSCEIVER_RESET_ATTR_ID(42),
    TRANSCEIVER_RESET_ATTR_ID(43),
    TRANSCEIVER_RESET_ATTR_ID(44),
    TRANSCEIVER_RESET_ATTR_ID(45),
    TRANSCEIVER_RESET_ATTR_ID(46),
    TRANSCEIVER_RESET_ATTR_ID(47),
    TRANSCEIVER_RESET_ATTR_ID(48),
    TRANSCEIVER_RESET_ATTR_ID(49),
    TRANSCEIVER_RESET_ATTR_ID(50),
    TRANSCEIVER_RESET_ATTR_ID(51),
    TRANSCEIVER_RESET_ATTR_ID(52),
    TRANSCEIVER_RESET_ATTR_ID(53),
    TRANSCEIVER_RESET_ATTR_ID(54),
    TRANSCEIVER_RESET_ATTR_ID(55),
    TRANSCEIVER_RESET_ATTR_ID(56),
    TRANSCEIVER_RESET_ATTR_ID(57),
    TRANSCEIVER_RESET_ATTR_ID(58),
    TRANSCEIVER_RESET_ATTR_ID(59),
    TRANSCEIVER_RESET_ATTR_ID(60),
    TRANSCEIVER_RESET_ATTR_ID(61),
    TRANSCEIVER_RESET_ATTR_ID(62),
    TRANSCEIVER_RESET_ATTR_ID(63),
    TRANSCEIVER_RESET_ATTR_ID(64),
    TRANSCEIVER_RESET_ATTR_ID(65),
    TRANSCEIVER_RESET_ATTR_ID(66),
    TRANSCEIVER_RESET_ATTR_ID(67),
    TRANSCEIVER_RESET_ATTR_ID(68),
    TRANSCEIVER_RESET_ATTR_ID(69),
    TRANSCEIVER_RESET_ATTR_ID(70),
    TRANSCEIVER_RESET_ATTR_ID(71),
    TRANSCEIVER_RESET_ATTR_ID(72),
    TRANSCEIVER_LPMODE_ATTR_ID(1),
    TRANSCEIVER_LPMODE_ATTR_ID(2),
    TRANSCEIVER_LPMODE_ATTR_ID(3),
    TRANSCEIVER_LPMODE_ATTR_ID(4),
    TRANSCEIVER_LPMODE_ATTR_ID(5),
    TRANSCEIVER_LPMODE_ATTR_ID(6),
    TRANSCEIVER_LPMODE_ATTR_ID(7),
    TRANSCEIVER_LPMODE_ATTR_ID(8),
    TRANSCEIVER_LPMODE_ATTR_ID(9),
    TRANSCEIVER_LPMODE_ATTR_ID(10),
    TRANSCEIVER_LPMODE_ATTR_ID(11),
    TRANSCEIVER_LPMODE_ATTR_ID(12),
    TRANSCEIVER_LPMODE_ATTR_ID(13),
    TRANSCEIVER_LPMODE_ATTR_ID(14),
    TRANSCEIVER_LPMODE_ATTR_ID(15),
    TRANSCEIVER_LPMODE_ATTR_ID(16),
    TRANSCEIVER_LPMODE_ATTR_ID(17),
    TRANSCEIVER_LPMODE_ATTR_ID(18),
    TRANSCEIVER_LPMODE_ATTR_ID(19),
    TRANSCEIVER_LPMODE_ATTR_ID(20),
    TRANSCEIVER_LPMODE_ATTR_ID(21),
    TRANSCEIVER_LPMODE_ATTR_ID(22),
    TRANSCEIVER_LPMODE_ATTR_ID(23),
    TRANSCEIVER_LPMODE_ATTR_ID(24),
    TRANSCEIVER_LPMODE_ATTR_ID(25),
    TRANSCEIVER_LPMODE_ATTR_ID(26),
    TRANSCEIVER_LPMODE_ATTR_ID(27),
    TRANSCEIVER_LPMODE_ATTR_ID(28),
    TRANSCEIVER_LPMODE_ATTR_ID(29),
    TRANSCEIVER_LPMODE_ATTR_ID(30),
    TRANSCEIVER_LPMODE_ATTR_ID(31),
    TRANSCEIVER_LPMODE_ATTR_ID(32),
    TRANSCEIVER_LPMODE_ATTR_ID(33),
    TRANSCEIVER_LPMODE_ATTR_ID(34),
    TRANSCEIVER_LPMODE_ATTR_ID(35),
    TRANSCEIVER_LPMODE_ATTR_ID(36),
    TRANSCEIVER_LPMODE_ATTR_ID(37),
    TRANSCEIVER_LPMODE_ATTR_ID(38),
    TRANSCEIVER_LPMODE_ATTR_ID(39),
    TRANSCEIVER_LPMODE_ATTR_ID(40),
    TRANSCEIVER_LPMODE_ATTR_ID(41),
    TRANSCEIVER_LPMODE_ATTR_ID(42),
    TRANSCEIVER_LPMODE_ATTR_ID(43),
    TRANSCEIVER_LPMODE_ATTR_ID(44),
    TRANSCEIVER_LPMODE_ATTR_ID(45),
    TRANSCEIVER_LPMODE_ATTR_ID(46),
    TRANSCEIVER_LPMODE_ATTR_ID(47),
    TRANSCEIVER_LPMODE_ATTR_ID(48),
    TRANSCEIVER_LPMODE_ATTR_ID(49),
    TRANSCEIVER_LPMODE_ATTR_ID(50),
    TRANSCEIVER_LPMODE_ATTR_ID(51),
    TRANSCEIVER_LPMODE_ATTR_ID(52),
    TRANSCEIVER_LPMODE_ATTR_ID(53),
    TRANSCEIVER_LPMODE_ATTR_ID(54),
    TRANSCEIVER_LPMODE_ATTR_ID(55),
    TRANSCEIVER_LPMODE_ATTR_ID(56),
    TRANSCEIVER_LPMODE_ATTR_ID(57),
    TRANSCEIVER_LPMODE_ATTR_ID(58),
    TRANSCEIVER_LPMODE_ATTR_ID(59),
    TRANSCEIVER_LPMODE_ATTR_ID(60),
    TRANSCEIVER_LPMODE_ATTR_ID(61),
    TRANSCEIVER_LPMODE_ATTR_ID(62),
    TRANSCEIVER_LPMODE_ATTR_ID(63),
    TRANSCEIVER_LPMODE_ATTR_ID(64),
    TRANSCEIVER_LPMODE_ATTR_ID(65),
    TRANSCEIVER_LPMODE_ATTR_ID(66),
    TRANSCEIVER_LPMODE_ATTR_ID(67),
    TRANSCEIVER_LPMODE_ATTR_ID(68),
    TRANSCEIVER_LPMODE_ATTR_ID(69),
    TRANSCEIVER_LPMODE_ATTR_ID(70),
    TRANSCEIVER_LPMODE_ATTR_ID(71),
    TRANSCEIVER_LPMODE_ATTR_ID(72),
    TRANSCEIVER_TX_DISABLE_ATTR_ID(73),
    TRANSCEIVER_TX_DISABLE_ATTR_ID(74),
    TRANSCEIVER_TX_DISABLE_ATTR_ID(75),
    TRANSCEIVER_TX_DISABLE_ATTR_ID(76),
    TRANSCEIVER_TX_FAULT_ATTR_ID(73),
    TRANSCEIVER_TX_FAULT_ATTR_ID(74),
    TRANSCEIVER_TX_FAULT_ATTR_ID(75),
    TRANSCEIVER_TX_FAULT_ATTR_ID(76),
    TRANSCEIVER_RX_LOS_ATTR_ID(73),
    TRANSCEIVER_RX_LOS_ATTR_ID(74),
    TRANSCEIVER_RX_LOS_ATTR_ID(75),
    TRANSCEIVER_RX_LOS_ATTR_ID(76),
};

/***********************************************
 *       function declare
 * *********************************************/
static ssize_t status_read(struct device *dev, struct device_attribute *da,
             char *buf);
static ssize_t status_write(struct device *dev, struct device_attribute *da,
            const char *buf, size_t count);

#define DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(index) \
    static SENSOR_DEVICE_ATTR(module_present_##index, S_IRUGO, status_read, NULL, MODULE_PRESENT_##index); \
    static SENSOR_DEVICE_ATTR(module_reset_##index, S_IRUGO|S_IWUSR, status_read, status_write, MODULE_RESET_##index); \
    static SENSOR_DEVICE_ATTR(module_lp_mode_##index, S_IRUGO|S_IWUSR, status_read, status_write, MODULE_LPMODE_##index)
#define DECLARE_TRANSCEIVER_ATTR(index) \
    &sensor_dev_attr_module_present_##index.dev_attr.attr, \
    &sensor_dev_attr_module_reset_##index.dev_attr.attr, \
    &sensor_dev_attr_module_lp_mode_##index.dev_attr.attr

/* transceiver attributes */
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(1);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(2);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(3);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(4);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(5);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(6);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(7);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(8);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(9);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(10);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(11);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(12);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(13);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(14);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(15);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(16);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(17);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(18);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(19);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(20);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(21);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(22);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(23);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(24);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(25);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(26);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(27);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(28);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(29);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(30);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(31);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(32);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(33);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(34);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(35);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(36);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(37);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(38);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(39);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(40);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(41);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(42);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(43);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(44);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(45);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(46);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(47);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(48);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(49);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(50);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(51);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(52);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(53);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(54);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(55);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(56);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(57);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(58);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(59);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(60);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(61);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(62);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(63);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(64);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(65);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(66);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(67);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(68);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(69);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(70);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(71);
DECLARE_TRANSCEIVER_SENSOR_DEVICE_ATTR(72);
static SENSOR_DEVICE_ATTR(module_present_73, S_IRUGO, status_read, NULL, MODULE_PRESENT_73);
static SENSOR_DEVICE_ATTR(module_present_74, S_IRUGO, status_read, NULL, MODULE_PRESENT_74);
static SENSOR_DEVICE_ATTR(module_present_75, S_IRUGO, status_read, NULL, MODULE_PRESENT_75);
static SENSOR_DEVICE_ATTR(module_present_76, S_IRUGO, status_read, NULL, MODULE_PRESENT_76);
static SENSOR_DEVICE_ATTR(module_tx_disable_73, S_IRUGO|S_IWUSR, status_read,
                          status_write, MODULE_TX_DISABLE_73);
static SENSOR_DEVICE_ATTR(module_tx_disable_74, S_IRUGO|S_IWUSR, status_read,
                          status_write, MODULE_TX_DISABLE_74);
static SENSOR_DEVICE_ATTR(module_tx_disable_75, S_IRUGO|S_IWUSR, status_read,
                          status_write, MODULE_TX_DISABLE_75);
static SENSOR_DEVICE_ATTR(module_tx_disable_76, S_IRUGO|S_IWUSR, status_read,
                          status_write, MODULE_TX_DISABLE_76);
static SENSOR_DEVICE_ATTR(module_tx_fault_73, S_IRUGO, status_read, NULL, MODULE_TX_FAULT_73);
static SENSOR_DEVICE_ATTR(module_tx_fault_74, S_IRUGO, status_read, NULL, MODULE_TX_FAULT_74);
static SENSOR_DEVICE_ATTR(module_tx_fault_75, S_IRUGO, status_read, NULL, MODULE_TX_FAULT_75);
static SENSOR_DEVICE_ATTR(module_tx_fault_76, S_IRUGO, status_read, NULL, MODULE_TX_FAULT_76);
static SENSOR_DEVICE_ATTR(module_rx_los_73, S_IRUGO, status_read, NULL, MODULE_RX_LOS_73);
static SENSOR_DEVICE_ATTR(module_rx_los_74, S_IRUGO, status_read, NULL, MODULE_RX_LOS_74);
static SENSOR_DEVICE_ATTR(module_rx_los_75, S_IRUGO, status_read, NULL, MODULE_RX_LOS_75);
static SENSOR_DEVICE_ATTR(module_rx_los_76, S_IRUGO, status_read, NULL, MODULE_RX_LOS_76);

static struct attribute *fpga_transceiver_attributes[] = {
    DECLARE_TRANSCEIVER_ATTR(1),
    DECLARE_TRANSCEIVER_ATTR(2),
    DECLARE_TRANSCEIVER_ATTR(3),
    DECLARE_TRANSCEIVER_ATTR(4),
    DECLARE_TRANSCEIVER_ATTR(5),
    DECLARE_TRANSCEIVER_ATTR(6),
    DECLARE_TRANSCEIVER_ATTR(7),
    DECLARE_TRANSCEIVER_ATTR(8),
    DECLARE_TRANSCEIVER_ATTR(9),
    DECLARE_TRANSCEIVER_ATTR(10),
    DECLARE_TRANSCEIVER_ATTR(11),
    DECLARE_TRANSCEIVER_ATTR(12),
    DECLARE_TRANSCEIVER_ATTR(13),
    DECLARE_TRANSCEIVER_ATTR(14),
    DECLARE_TRANSCEIVER_ATTR(15),
    DECLARE_TRANSCEIVER_ATTR(16),
    DECLARE_TRANSCEIVER_ATTR(17),
    DECLARE_TRANSCEIVER_ATTR(18),
    DECLARE_TRANSCEIVER_ATTR(19),
    DECLARE_TRANSCEIVER_ATTR(20),
    DECLARE_TRANSCEIVER_ATTR(21),
    DECLARE_TRANSCEIVER_ATTR(22),
    DECLARE_TRANSCEIVER_ATTR(23),
    DECLARE_TRANSCEIVER_ATTR(24),
    DECLARE_TRANSCEIVER_ATTR(25),
    DECLARE_TRANSCEIVER_ATTR(26),
    DECLARE_TRANSCEIVER_ATTR(27),
    DECLARE_TRANSCEIVER_ATTR(28),
    DECLARE_TRANSCEIVER_ATTR(29),
    DECLARE_TRANSCEIVER_ATTR(30),
    DECLARE_TRANSCEIVER_ATTR(31),
    DECLARE_TRANSCEIVER_ATTR(32),
    DECLARE_TRANSCEIVER_ATTR(33),
    DECLARE_TRANSCEIVER_ATTR(34),
    DECLARE_TRANSCEIVER_ATTR(35),
    DECLARE_TRANSCEIVER_ATTR(36),
    DECLARE_TRANSCEIVER_ATTR(37),
    DECLARE_TRANSCEIVER_ATTR(38),
    DECLARE_TRANSCEIVER_ATTR(39),
    DECLARE_TRANSCEIVER_ATTR(40),
    DECLARE_TRANSCEIVER_ATTR(41),
    DECLARE_TRANSCEIVER_ATTR(42),
    DECLARE_TRANSCEIVER_ATTR(43),
    DECLARE_TRANSCEIVER_ATTR(44),
    DECLARE_TRANSCEIVER_ATTR(45),
    DECLARE_TRANSCEIVER_ATTR(46),
    DECLARE_TRANSCEIVER_ATTR(47),
    DECLARE_TRANSCEIVER_ATTR(48),
    DECLARE_TRANSCEIVER_ATTR(49),
    DECLARE_TRANSCEIVER_ATTR(50),
    DECLARE_TRANSCEIVER_ATTR(51),
    DECLARE_TRANSCEIVER_ATTR(52),
    DECLARE_TRANSCEIVER_ATTR(53),
    DECLARE_TRANSCEIVER_ATTR(54),
    DECLARE_TRANSCEIVER_ATTR(55),
    DECLARE_TRANSCEIVER_ATTR(56),
    DECLARE_TRANSCEIVER_ATTR(57),
    DECLARE_TRANSCEIVER_ATTR(58),
    DECLARE_TRANSCEIVER_ATTR(59),
    DECLARE_TRANSCEIVER_ATTR(60),
    DECLARE_TRANSCEIVER_ATTR(61),
    DECLARE_TRANSCEIVER_ATTR(62),
    DECLARE_TRANSCEIVER_ATTR(63),
    DECLARE_TRANSCEIVER_ATTR(64),
    DECLARE_TRANSCEIVER_ATTR(65),
    DECLARE_TRANSCEIVER_ATTR(66),
    DECLARE_TRANSCEIVER_ATTR(67),
    DECLARE_TRANSCEIVER_ATTR(68),
    DECLARE_TRANSCEIVER_ATTR(69),
    DECLARE_TRANSCEIVER_ATTR(70),
    DECLARE_TRANSCEIVER_ATTR(71),
    DECLARE_TRANSCEIVER_ATTR(72),
    &sensor_dev_attr_module_present_73.dev_attr.attr,
    &sensor_dev_attr_module_present_74.dev_attr.attr,
    &sensor_dev_attr_module_present_75.dev_attr.attr,
    &sensor_dev_attr_module_present_76.dev_attr.attr,
    &sensor_dev_attr_module_tx_disable_73.dev_attr.attr,
    &sensor_dev_attr_module_tx_disable_74.dev_attr.attr,
    &sensor_dev_attr_module_tx_disable_75.dev_attr.attr,
    &sensor_dev_attr_module_tx_disable_76.dev_attr.attr,
    &sensor_dev_attr_module_tx_fault_73.dev_attr.attr,
    &sensor_dev_attr_module_tx_fault_74.dev_attr.attr,
    &sensor_dev_attr_module_tx_fault_75.dev_attr.attr,
    &sensor_dev_attr_module_tx_fault_76.dev_attr.attr,
    &sensor_dev_attr_module_rx_los_73.dev_attr.attr,
    &sensor_dev_attr_module_rx_los_74.dev_attr.attr,
    &sensor_dev_attr_module_rx_los_75.dev_attr.attr,
    &sensor_dev_attr_module_rx_los_76.dev_attr.attr,
    NULL
};

static const struct attribute_group fpga_port_stat_group = {
    .attrs = fpga_transceiver_attributes,
};

struct attribute_mapping {
    u16 attr_base;
    u16 reg;
    u8 bar;
    u8 revert;
};

// Define an array of attribute mappings
static struct attribute_mapping attribute_mappings[] = {
    /* QSFP28 */
    [MODULE_PRESENT_1 ... MODULE_PRESENT_8] = {MODULE_PRESENT_1, QSFP28_P7_P0_PRESENT_REG, BAR5_NUM, 1},
    [MODULE_PRESENT_9 ... MODULE_PRESENT_16] = {MODULE_PRESENT_9, QSFP28_P15_P8_PRESENT_REG, BAR5_NUM, 1},
    [MODULE_PRESENT_17 ... MODULE_PRESENT_24] = {MODULE_PRESENT_17, QSFP28_P23_P16_PRESENT_REG, BAR5_NUM, 1},
    [MODULE_PRESENT_25 ... MODULE_PRESENT_32] = {MODULE_PRESENT_25, QSFP28_P31_P24_PRESENT_REG, BAR5_NUM, 1},
    [MODULE_PRESENT_33 ... MODULE_PRESENT_40] = {MODULE_PRESENT_33, QSFP28_P39_P32_PRESENT_REG, BAR5_NUM, 1},
    [MODULE_PRESENT_41 ... MODULE_PRESENT_48] = {MODULE_PRESENT_41, QSFP28_P47_P40_PRESENT_REG, BAR5_NUM, 1},
    /* QSFP-DD */
    [MODULE_PRESENT_49 ... MODULE_PRESENT_56] = {MODULE_PRESENT_49, QSFP_DD_P7_P0_PRESENT_REG, BAR4_NUM, 1},
    [MODULE_PRESENT_57 ... MODULE_PRESENT_60] = {MODULE_PRESENT_57, QSFP_DD_P11_P8_PRESENT_REG, BAR4_NUM, 1},
    [MODULE_PRESENT_61 ... MODULE_PRESENT_68] = {MODULE_PRESENT_61, QSFP_DD_P19_P12_PRESENT_REG, BAR4_NUM, 1},
    [MODULE_PRESENT_69 ... MODULE_PRESENT_72] = {MODULE_PRESENT_69, QSFP_DD_P23_P20_PRESENT_REG, BAR4_NUM, 1},
    /* SFP */
    [MODULE_PRESENT_73 ... MODULE_PRESENT_76] = {MODULE_PRESENT_73, SFP_P3_P0_PRESENT_REG, BAR5_NUM, 1},
     /* QSFP28 */
    [MODULE_RESET_1 ... MODULE_RESET_8] = {MODULE_RESET_1, QSFP28_P7_P0_RESET_REG, BAR5_NUM, 1},
    [MODULE_RESET_9 ... MODULE_RESET_16] = {MODULE_RESET_9, QSFP28_P15_P8_RESET_REG, BAR5_NUM, 1},
    [MODULE_RESET_17 ... MODULE_RESET_24] = {MODULE_RESET_17, QSFP28_P23_P16_RESET_REG, BAR5_NUM, 1},
    [MODULE_RESET_25 ... MODULE_RESET_32] = {MODULE_RESET_25, QSFP28_P31_P24_RESET_REG, BAR5_NUM, 1},
    [MODULE_RESET_33 ... MODULE_RESET_40] = {MODULE_RESET_33, QSFP28_P39_P32_RESET_REG, BAR5_NUM, 1},
    [MODULE_RESET_41 ... MODULE_RESET_48] = {MODULE_RESET_41, QSFP28_P47_P40_RESET_REG, BAR5_NUM, 1},
    /* QSFP-DD */
    [MODULE_RESET_49 ... MODULE_RESET_56] = {MODULE_RESET_49, QSFP_DD_P7_P0_RESET_REG, BAR4_NUM, 1},
    [MODULE_RESET_57 ... MODULE_RESET_60] = {MODULE_RESET_57, QSFP_DD_P11_P8_RESET_REG, BAR4_NUM, 1},
    [MODULE_RESET_61 ... MODULE_RESET_68] = {MODULE_RESET_61, QSFP_DD_P19_P12_RESET_REG, BAR4_NUM, 1},
    [MODULE_RESET_69 ... MODULE_RESET_72] = {MODULE_RESET_69, QSFP_DD_P23_P20_RESET_REG, BAR4_NUM, 1},
    /* QSFP28 */
    [MODULE_LPMODE_1 ... MODULE_LPMODE_8] = {MODULE_LPMODE_1, QSFP28_P7_P0_LPMODE_REG, BAR5_NUM, 0},
    [MODULE_LPMODE_9 ... MODULE_LPMODE_16] = {MODULE_LPMODE_9, QSFP28_P15_P8_LPMODE_REG, BAR5_NUM, 0},
    [MODULE_LPMODE_17 ... MODULE_LPMODE_24] = {MODULE_LPMODE_17, QSFP28_P23_P16_LPMODE_REG, BAR5_NUM, 0},
    [MODULE_LPMODE_25 ... MODULE_LPMODE_32] = {MODULE_LPMODE_25, QSFP28_P31_P24_LPMODE_REG, BAR5_NUM, 0},
    [MODULE_LPMODE_33 ... MODULE_LPMODE_40] = {MODULE_LPMODE_33, QSFP28_P39_P32_LPMODE_REG, BAR5_NUM, 0},
    [MODULE_LPMODE_41 ... MODULE_LPMODE_48] = {MODULE_LPMODE_41, QSFP28_P47_P40_LPMODE_REG, BAR5_NUM, 0},
    /* QSFP-DD */
    [MODULE_LPMODE_49 ... MODULE_LPMODE_56] = {MODULE_LPMODE_49, QSFP_DD_P7_P0_LPMODE_REG, BAR4_NUM, 0},
    [MODULE_LPMODE_57 ... MODULE_LPMODE_60] = {MODULE_LPMODE_57, QSFP_DD_P11_P8_LPMODE_REG, BAR4_NUM, 0},
    [MODULE_LPMODE_61 ... MODULE_LPMODE_68] = {MODULE_LPMODE_61, QSFP_DD_P19_P12_LPMODE_REG, BAR4_NUM, 0},
    [MODULE_LPMODE_69 ... MODULE_LPMODE_72] = {MODULE_LPMODE_69, QSFP_DD_P23_P20_LPMODE_REG, BAR4_NUM, 0},

    /* SFP TXDIS, TXFAULT, RXLOS*/
    [MODULE_TX_DISABLE_73 ... MODULE_TX_DISABLE_76] ={MODULE_TX_DISABLE_73, SFP_P3_P0_TXDIS_REG,BAR5_NUM,  0},
    [MODULE_TX_FAULT_73 ... MODULE_TX_FAULT_76] = {MODULE_TX_FAULT_73, SFP_P3_P0_TXFAULT_REG, BAR5_NUM, 0},
    [MODULE_RX_LOS_73 ... MODULE_RX_LOS_76] = {MODULE_RX_LOS_73, SFP_P3_P0_RXLOS_REG, BAR5_NUM, 1},
};

static inline unsigned int fpga_read(void __iomem *addr, u32 spi_mask)
{
    wait_spi(spi_mask, usecs_to_jiffies(20));
    return ioread8(addr);
}

static inline void fpga_write(void __iomem *addr, u8 val, u32 spi_mask)
{
    wait_spi(spi_mask, usecs_to_jiffies(20));
    iowrite8(val, addr);
}

static ssize_t status_read(struct device *dev, struct device_attribute *da, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct as9947_72xkb_fpga_data *fpga_ctl = dev_get_drvdata(dev);
    ssize_t ret = -EINVAL;
    u16 reg;
    u8 fpga_async_data, reg_val, bar;
    u8 bits_shift;

    switch(attr->index)
    {
        case MODULE_PRESENT_1 ... MODULE_PRESENT_16:
            fpga_async_data = 0x0;
            break;
        case MODULE_PRESENT_17 ... MODULE_PRESENT_32:
            fpga_async_data = 0x20;
            break;
        case MODULE_PRESENT_33 ... MODULE_PRESENT_48:
            fpga_async_data = 0x10;
            break;
        case MODULE_PRESENT_49 ... MODULE_PRESENT_60:
            fpga_async_data = 0x0;
            break;
        case MODULE_PRESENT_61 ... MODULE_PRESENT_72:
            fpga_async_data = 0x01;
            break;
        case MODULE_PRESENT_73 ... MODULE_PRESENT_76:
            fpga_async_data = 0x10;
            break;
        case MODULE_LPMODE_1 ... MODULE_LPMODE_16:
            fpga_async_data = 0x0;
            break;
        case MODULE_LPMODE_17 ... MODULE_LPMODE_32:
            fpga_async_data = 0x20;
            break;
        case MODULE_LPMODE_33 ... MODULE_LPMODE_48:
            fpga_async_data = 0x10;
            break;
        case MODULE_LPMODE_49 ... MODULE_LPMODE_60:
            fpga_async_data = 0x0;
            break;
        case MODULE_LPMODE_61 ... MODULE_LPMODE_72:
            fpga_async_data = 0x01;
            break;
        case MODULE_RESET_1 ... MODULE_RESET_16:
            fpga_async_data = 0x0;
            break;
        case MODULE_RESET_17 ... MODULE_RESET_32:
            fpga_async_data = 0x20;
            break;
        case MODULE_RESET_33 ... MODULE_RESET_48:
            fpga_async_data = 0x10;
            break;
        case MODULE_RESET_49 ... MODULE_RESET_60:
            fpga_async_data = 0x0;
            break;
        case MODULE_RESET_61 ... MODULE_RESET_72:
            fpga_async_data = 0x01;
            break;
        case MODULE_TX_DISABLE_73 ... MODULE_RX_LOS_76:
            fpga_async_data = 0x10;
            break;
        default:
            break;
    }

    reg = attribute_mappings[attr->index].reg;
    bar = attribute_mappings[attr->index].bar;

    LOCK(&cpld_access_lock);
    if (bar == BAR4_NUM){
        iowrite8(fpga_async_data, async_reg);
        reg_val = fpga_read(fpga_ctl->pci_fpga_dev.data_base_addr4 + reg,
                            SPI_BUSY_MASK_CPLD1);
    }
    else
    {
        iowrite8(fpga_async_data, async_reg);
        reg_val = fpga_read(fpga_ctl->pci_fpga_dev.data_base_addr5 + reg,
                            SPI_BUSY_MASK_CPLD2);
    }
    UNLOCK(&cpld_access_lock);
    bits_shift = attr->index - attribute_mappings[attr->index].attr_base;
    reg_val = (reg_val >> bits_shift) & 0x01;

    if (attribute_mappings[attr->index].revert) 
        reg_val = !reg_val;
    ret = sprintf(buf, "%u\n", reg_val);

    return ret;
}

static ssize_t status_write(struct device *dev, struct device_attribute *da,
                            const char *buf, size_t count)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct as9947_72xkb_fpga_data *fpga_ctl = dev_get_drvdata(dev);
    void __iomem *addr;
    int status;
    u16 reg, bar;
    u8 input, fpga_async_data;
    u8 reg_val, bit_mask, should_set_bit;
    u32 spi_mask;

    status = kstrtou8(buf, 10, &input);
    if (status) {
        return status;
    }

    reg = attribute_mappings[attr->index].reg;
    bar = attribute_mappings[attr->index].bar;
    switch(attr->index)
    {
        case MODULE_LPMODE_1 ... MODULE_LPMODE_16:
            fpga_async_data = 0x0;
            break;
        case MODULE_LPMODE_17 ... MODULE_LPMODE_32:
            fpga_async_data = 0x20;
            break;
        case MODULE_LPMODE_33 ... MODULE_LPMODE_48:
            fpga_async_data = 0x10;
            break;
        case MODULE_LPMODE_49 ... MODULE_LPMODE_60:
            fpga_async_data = 0x0;
            break;
        case MODULE_LPMODE_61 ... MODULE_LPMODE_72:
            fpga_async_data = 0x01;
            break;
        case MODULE_RESET_1 ... MODULE_RESET_16:
            fpga_async_data = 0x0;
            break;
        case MODULE_RESET_17 ... MODULE_RESET_32:
            fpga_async_data = 0x20;
            break;
        case MODULE_RESET_33 ... MODULE_RESET_48:
            fpga_async_data = 0x10;
            break;
        case MODULE_RESET_49 ... MODULE_RESET_60:
            fpga_async_data = 0x0;
            break;
        case MODULE_RESET_61 ... MODULE_RESET_72:
            fpga_async_data = 0x01;
            break;
        case MODULE_TX_DISABLE_73 ... MODULE_RX_LOS_76:
            fpga_async_data = 0x10;
            break;
        default:
            break;
    }

    if (bar == BAR4_NUM){
        spi_mask = SPI_BUSY_MASK_CPLD1;
        addr = fpga_ctl->pci_fpga_dev.data_base_addr4;
    } 
    else 
    {
        spi_mask = SPI_BUSY_MASK_CPLD2;
        addr = fpga_ctl->pci_fpga_dev.data_base_addr5;
    }

    bit_mask = 0x01 << (attr->index - attribute_mappings[attr->index].attr_base);
    should_set_bit = attribute_mappings[attr->index].revert ? !input : input;

    LOCK(&cpld_access_lock);
    iowrite8(fpga_async_data, async_reg);
    reg_val = fpga_read(addr + reg, spi_mask);
    if (should_set_bit) {
        reg_val |= bit_mask;
    } else {
        reg_val &= ~bit_mask;
    }
    fpga_write(addr + reg, reg_val, spi_mask);
    UNLOCK(&cpld_access_lock);

    return count;
}

struct _port_data {
    u16 offset;
    u16 mask; /* SPI Busy mask : 0x01 --> CPLD1, 0x02 --> CPLD2 */
};
/* ============PCIe Bar Offset to I2C Master Mapping============== */
static const struct _port_data port[PORT_NUM]= {
    /* QSFP Port 0-15, Region 5, PCIE to MEZZ_CPLD0 */
    {0x2100, SPI_BUSY_MASK_CPLD2},/* 0x2100 - 0x2110  I2C Master QSFP28 Port0 */
    {0x2120, SPI_BUSY_MASK_CPLD2},/* 0x2120 - 0x2130  I2C Master QSFP28 Port1 */
    {0x2140, SPI_BUSY_MASK_CPLD2},/* 0x2140 - 0x2150  I2C Master QSFP28 Port2 */
    {0x2160, SPI_BUSY_MASK_CPLD2},/* 0x2160 - 0x2170  I2C Master QSFP28 Port3 */
    {0x2180, SPI_BUSY_MASK_CPLD2},/* 0x2180 - 0x2190  I2C Master QSFP28 Port4 */
    {0x21A0, SPI_BUSY_MASK_CPLD2},/* 0x21A0 - 0x21B0  I2C Master QSFP28 Port5 */
    {0x21C0, SPI_BUSY_MASK_CPLD2},/* 0x21C0 - 0x21D0  I2C Master QSFP28 Port6 */
    {0x21E0, SPI_BUSY_MASK_CPLD2},/* 0x21E0 - 0x21F0  I2C Master QSFP28 Port7 */
    {0x2200, SPI_BUSY_MASK_CPLD2},/* 0x2200 - 0x2210  I2C Master QSFP28 Port8 */
    {0x2220, SPI_BUSY_MASK_CPLD2},/* 0x2220 - 0x2230  I2C Master QSFP28 Port9 */
    {0x2240, SPI_BUSY_MASK_CPLD2},/* 0x2240 - 0x2250  I2C Master QSFP28 Port10 */
    {0x2260, SPI_BUSY_MASK_CPLD2},/* 0x2260 - 0x2270  I2C Master QSFP28 Port11 */
    {0x2280, SPI_BUSY_MASK_CPLD2},/* 0x2280 - 0x2290  I2C Master QSFP28 Port12 */
    {0x22A0, SPI_BUSY_MASK_CPLD2},/* 0x22A0 - 0x22B0  I2C Master QSFP28 Port13 */
    {0x22C0, SPI_BUSY_MASK_CPLD2},/* 0x22C0 - 0x22D0  I2C Master QSFP28 Port14 */
    {0x22E0, SPI_BUSY_MASK_CPLD2},/* 0x22E0 - 0x22F0  I2C Master QSFP28 Port15 */
    /* QSFP Port 16-31, Region 5, PCIE to MEZZ_CPLD2 */
    {0x2100, SPI_BUSY_MASK_CPLD2},/* 0x2100 - 0x2110  I2C Master QSFP28 Port16 */
    {0x2120, SPI_BUSY_MASK_CPLD2},/* 0x2120 - 0x2130  I2C Master QSFP28 Port17*/
    {0x2140, SPI_BUSY_MASK_CPLD2},/* 0x2140 - 0x2150  I2C Master QSFP28 Port18 */
    {0x2160, SPI_BUSY_MASK_CPLD2},/* 0x2160 - 0x2170  I2C Master QSFP28 Port19 */
    {0x2180, SPI_BUSY_MASK_CPLD2},/* 0x2180 - 0x2190  I2C Master QSFP28 Port20 */
    {0x21A0, SPI_BUSY_MASK_CPLD2},/* 0x21A0 - 0x21B0  I2C Master QSFP28 Port21 */
    {0x21C0, SPI_BUSY_MASK_CPLD2},/* 0x21C0 - 0x21D0  I2C Master QSFP28 Port22 */
    {0x21E0, SPI_BUSY_MASK_CPLD2},/* 0x21E0 - 0x21F0  I2C Master QSFP28 Port23 */
    {0x2200, SPI_BUSY_MASK_CPLD2},/* 0x2200 - 0x2210  I2C Master QSFP28 Port24 */
    {0x2220, SPI_BUSY_MASK_CPLD2},/* 0x2220 - 0x2230  I2C Master QSFP28 Port25 */
    {0x2240, SPI_BUSY_MASK_CPLD2},/* 0x2240 - 0x2250  I2C Master QSFP28 Port26 */
    {0x2260, SPI_BUSY_MASK_CPLD2},/* 0x2260 - 0x2270  I2C Master QSFP28 Port27 */
    {0x2280, SPI_BUSY_MASK_CPLD2},/* 0x2280 - 0x2290  I2C Master QSFP28 Port28 */
    {0x22A0, SPI_BUSY_MASK_CPLD2},/* 0x22A0 - 0x22B0  I2C Master QSFP28 Port29 */
    {0x22C0, SPI_BUSY_MASK_CPLD2},/* 0x22C0 - 0x22D0  I2C Master QSFP28 Port30 */
    {0x22E0, SPI_BUSY_MASK_CPLD2},/* 0x22E0 - 0x22F0  I2C Master QSFP28 Port31 */
    /* QSFP Port 32-47, Region 5, PCIE to MEZZ_CPLD1 */
    {0x2100, SPI_BUSY_MASK_CPLD2},/* 0x2100 - 0x2110  I2C Master QSFP28 Port32 */
    {0x2120, SPI_BUSY_MASK_CPLD2},/* 0x2120 - 0x2130  I2C Master QSFP28 Port33 */
    {0x2140, SPI_BUSY_MASK_CPLD2},/* 0x2140 - 0x2150  I2C Master QSFP28 Port34 */
    {0x2160, SPI_BUSY_MASK_CPLD2},/* 0x2160 - 0x2170  I2C Master QSFP28 Port35 */
    {0x2180, SPI_BUSY_MASK_CPLD2},/* 0x2180 - 0x2190  I2C Master QSFP28 Port36 */
    {0x21A0, SPI_BUSY_MASK_CPLD2},/* 0x21A0 - 0x21B0  I2C Master QSFP28 Port37 */
    {0x21C0, SPI_BUSY_MASK_CPLD2},/* 0x21C0 - 0x21D0  I2C Master QSFP28 Port38 */
    {0x21E0, SPI_BUSY_MASK_CPLD2},/* 0x21E0 - 0x21F0  I2C Master QSFP28 Port39 */
    {0x2200, SPI_BUSY_MASK_CPLD2},/* 0x2200 - 0x2210  I2C Master QSFP28 Port40 */
    {0x2220, SPI_BUSY_MASK_CPLD2},/* 0x2220 - 0x2230  I2C Master QSFP28 Port41 */
    {0x2240, SPI_BUSY_MASK_CPLD2},/* 0x2240 - 0x2250  I2C Master QSFP28 Port42 */
    {0x2260, SPI_BUSY_MASK_CPLD2},/* 0x2260 - 0x2270  I2C Master QSFP28 Port43 */
    {0x2280, SPI_BUSY_MASK_CPLD2},/* 0x2280 - 0x2290  I2C Master QSFP28 Port44 */
    {0x22A0, SPI_BUSY_MASK_CPLD2},/* 0x22A0 - 0x22B0  I2C Master QSFP28 Port45 */
    {0x22C0, SPI_BUSY_MASK_CPLD2},/* 0x22C0 - 0x22D0  I2C Master QSFP28 Port46 */
    {0x22E0, SPI_BUSY_MASK_CPLD2},/* 0x22E0 - 0x22F0  I2C Master QSFP28 Port47 */
    /* QSFP-DD Port48-59, Region 4, PCIE to MB_CPLD0 */
    {0x2100, SPI_BUSY_MASK_CPLD1},/* 0x2100 - 0x2110  I2C Master QSFP-DD Port48 */
    {0x2120, SPI_BUSY_MASK_CPLD1},/* 0x2120 - 0x2130  I2C Master QSFP-DD Port49 */
    {0x2140, SPI_BUSY_MASK_CPLD1},/* 0x2140 - 0x2150  I2C Master QSFP-DD Port50 */
    {0x2160, SPI_BUSY_MASK_CPLD1},/* 0x2160 - 0x2170  I2C Master QSFP-DD Port51 */
    {0x2180, SPI_BUSY_MASK_CPLD1},/* 0x2180 - 0x2190  I2C Master QSFP-DD Port52 */
    {0x21A0, SPI_BUSY_MASK_CPLD1},/* 0x21A0 - 0x21B0  I2C Master QSFP-DD Port53 */
    {0x21C0, SPI_BUSY_MASK_CPLD1},/* 0x21C0 - 0x21D0  I2C Master QSFP-DD Port54 */
    {0x21E0, SPI_BUSY_MASK_CPLD1},/* 0x21E0 - 0x21F0  I2C Master QSFP-DD Port55 */
    {0x2200, SPI_BUSY_MASK_CPLD1},/* 0x2200 - 0x2210  I2C Master QSFP-DD Port56 */
    {0x2220, SPI_BUSY_MASK_CPLD1},/* 0x2220 - 0x2230  I2C Master QSFP-DD Port57 */
    {0x2240, SPI_BUSY_MASK_CPLD1},/* 0x2240 - 0x2250  I2C Master QSFP-DD Port58 */
    {0x2260, SPI_BUSY_MASK_CPLD1},/* 0x2260 - 0x2270  I2C Master QSFP-DD Port59 */
    /* QSFP-DD Port60-71, Region 4, PCIE to MB_CPLD1 */
    {0x2100, SPI_BUSY_MASK_CPLD1},/* 0x2100 - 0x2110  I2C Master QSFP-DD Port60 */
    {0x2120, SPI_BUSY_MASK_CPLD1},/* 0x2120 - 0x2130  I2C Master QSFP-DD Port61 */
    {0x2140, SPI_BUSY_MASK_CPLD1},/* 0x2140 - 0x2150  I2C Master QSFP-DD Port62 */
    {0x2160, SPI_BUSY_MASK_CPLD1},/* 0x2160 - 0x2170  I2C Master QSFP-DD Port63 */
    {0x2180, SPI_BUSY_MASK_CPLD1},/* 0x2180 - 0x2190  I2C Master QSFP-DD Port64 */
    {0x21A0, SPI_BUSY_MASK_CPLD1},/* 0x21A0 - 0x21B0  I2C Master QSFP-DD Port65 */
    {0x21C0, SPI_BUSY_MASK_CPLD1},/* 0x21C0 - 0x21D0  I2C Master QSFP-DD Port66 */
    {0x21E0, SPI_BUSY_MASK_CPLD1},/* 0x21E0 - 0x21F0  I2C Master QSFP-DD Port67 */
    {0x2200, SPI_BUSY_MASK_CPLD1},/* 0x2200 - 0x2210  I2C Master QSFP-DD Port68 */
    {0x2220, SPI_BUSY_MASK_CPLD1},/* 0x2220 - 0x2230  I2C Master QSFP-DD Port69 */
    {0x2240, SPI_BUSY_MASK_CPLD1},/* 0x2240 - 0x2250  I2C Master QSFP-DD Port70 */
    {0x2260, SPI_BUSY_MASK_CPLD1},/* 0x2260 - 0x2270  I2C Master QSFP-DD Port71 */
    /* SFP port 72-75, Region 5, PCIE to MEZZ_CPLD1 */
    {0x2300, SPI_BUSY_MASK_CPLD2},/* 0x2300 - 0x2310  I2C Master SFP+ Port72 */
    {0x2320, SPI_BUSY_MASK_CPLD2},/* 0x2320 - 0x2330  I2C Master SFP+ Port73 */
    {0x2340, SPI_BUSY_MASK_CPLD2},/* 0x2340 - 0x2350  I2C Master SFP+ Port74 */
    {0x2360, SPI_BUSY_MASK_CPLD2},/* 0x2360 - 0x2370  I2C Master SFP+ Port75 */

};

static struct ocores_i2c_platform_data as9947_72xkb_platform_data = {
    .reg_io_width = 1,
    .reg_shift = 2,
    /*
     * PRER_L and PRER_H are calculated based on clock_khz and bus_khz
     * in i2c-ocores.c:ocores_init.
     */

    /* SCL 400KHZ in FPGA spec. => PRER_L = 0x0B, PRER_H = 0x00 */
    .clock_khz = 24000,
    .bus_khz = 400,

    /* SCL 100KHZ in FPGA spec. => PRER_L = 0x2F, PRER_H = 0x00 */
    /*.clock_khz = 24000,*/
    /*.bus_khz = 100,*/

};

struct platform_device *ocore_i2c_device_add(unsigned int id, unsigned long bar_base,
                                             unsigned int offset)
{
    struct resource res = DEFINE_RES_MEM(bar_base + offset, 0x20);
    struct platform_device *pdev;
    int err;

    pdev = platform_device_alloc(OCORES_I2C_DRVNAME, id);
    if (!pdev) {
        err = -ENOMEM;
        pcie_err("Port%u device allocation failed (%d)\n", (id & 0xFF), err);
        goto exit;
    }

    err = platform_device_add_resources(pdev, &res, 1);
    if (err) {
        pcie_err("Port%u device resource addition failed (%d)\n", (id & 0xFF), err);
        goto exit_device_put;
    }

    err = platform_device_add_data(pdev, &as9947_72xkb_platform_data,
                       sizeof(struct ocores_i2c_platform_data));
    if (err) {
        pcie_err("Port%u platform data allocation failed (%d)\n", (id & 0xFF), err);
        goto exit_device_put;
    }

    err = platform_device_add(pdev);
    if (err) {
        pcie_err("Port%u device addition failed (%d)\n", (id & 0xFF), err);
        goto exit_device_put;
    }

    return pdev;

exit_device_put:
    platform_device_put(pdev);
exit:
    return NULL;
}

static int as9947_72xkb_pcie_fpga_stat_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct as9947_72xkb_fpga_data *fpga_ctl;
    struct pci_dev *pcidev;
    struct resource *ret;
    int i;
    int status = 0, err = 0;
    unsigned long bar_base;
    int fpga_async_data;

    fpga_ctl = devm_kzalloc(dev, sizeof(struct as9947_72xkb_fpga_data), GFP_KERNEL);
    if (!fpga_ctl) {
        return -ENOMEM;
    }
    platform_set_drvdata(pdev, fpga_ctl);

    pcidev = pci_get_device(FPGA_PCI_VENDOR_ID, FPGA_PCI_DEVICE_ID, NULL);
     if (!pcidev) {
        dev_err(dev, "Cannot found PCI device(%x:%x)\n",
                     FPGA_PCI_VENDOR_ID, FPGA_PCI_DEVICE_ID);
        return -ENODEV;
    }
    fpga_ctl->pci_fpga_dev.pci_dev = pcidev;

    err = pci_enable_device(pcidev);
    if (err != 0) {
        dev_err(dev, "Cannot enable PCI device(%x:%x)\n",
                     FPGA_PCI_VENDOR_ID, FPGA_PCI_DEVICE_ID);
        status = -ENODEV;
        goto exit_pci_disable;
    }
    /* enable PCI bus-mastering */
    pci_set_master(pcidev);
    /*
     * Cannot use 'pci_request_regions(pcidev, DRVNAME)'
     * to request all Region 4, Region 5 because another
     * address will be allocated by the i2c-ocores.ko.
     */
    fpga_ctl->pci_fpga_dev.data_base_addr0 = pci_iomap(pcidev, BAR0_NUM, 0);
    if (fpga_ctl->pci_fpga_dev.data_base_addr0 == NULL) {
        dev_err(dev, "Failed to map BAR0\n");
        status = -EIO;
        goto exit_pci_disable;
    }

    spi_busy_reg = fpga_ctl->pci_fpga_dev.data_base_addr0 + 0x30;
    async_reg = fpga_ctl->pci_fpga_dev.data_base_addr0 + 0x05;

    fpga_ctl->pci_fpga_dev.data_base_addr4 = pci_iomap(pcidev, BAR4_NUM, 0);
    if (fpga_ctl->pci_fpga_dev.data_base_addr4 == NULL) {
        dev_err(dev, "Failed to map BAR4\n");
        status = -EIO;
        goto exit_pci_iounmap0;
    }

    fpga_ctl->pci_fpga_dev.data_region4 = pci_resource_start(pcidev, BAR4_NUM) + CPLD_PCIE_START_OFFSET;
    ret = request_mem_region(fpga_ctl->pci_fpga_dev.data_region4, REGION_LEN, DRVNAME"_mb_cpld");
    if (ret == NULL) {
        dev_err(dev, "[%s] cannot request region\n", DRVNAME"_mb_cpld");
        status = -EIO;
        goto exit_pci_iounmap1;
    }
    dev_info(dev, "(BAR%d resource: Start=0x%lx, Length=0x%x)", BAR4_NUM,
                  (unsigned long)fpga_ctl->pci_fpga_dev.data_region4, REGION_LEN);

    fpga_ctl->pci_fpga_dev.data_base_addr5 = pci_iomap(pcidev, BAR5_NUM, 0);
    if (fpga_ctl->pci_fpga_dev.data_base_addr5 == NULL) {
        dev_err(dev, "Failed to map BAR5\n");
        status = -EIO;
        goto exit_pci_release1;
    }
    fpga_ctl->pci_fpga_dev.data_region5 = pci_resource_start(pcidev, BAR5_NUM) + CPLD_PCIE_START_OFFSET;
    ret = request_mem_region(fpga_ctl->pci_fpga_dev.data_region5, REGION_LEN, DRVNAME"_mezz_cpld");
    if (ret == NULL) {
        dev_err(dev, "[%s] cannot request region\n", DRVNAME"_mezz_cpld");
        status = -EIO;
        goto exit_pci_iounmap2;
    }
    dev_info(dev, "(BAR%d resource: Start=0x%lx, Length=0x%x)", BAR5_NUM,
                  (unsigned long)fpga_ctl->pci_fpga_dev.data_region5, REGION_LEN);
    /* Create I2C ocore devices first, then create the FPGA sysfs.
     * To prevent the application from accessing an ocore device
     * that has not been fully created due to the port status
     * being present.
     */

    /*
     * Create ocore_i2c device for QSFP/QSFP-DD/SFP EEPROM
     */
    for (i = 0; i < PORT_NUM; i++) {
        switch (i)
        {
            case 0 ... 15:
                /* bit 4-5,  2'b00: pcie to Mezz_CPLD0 */
                fpga_async_data = 0x00;
                bar_base = pci_resource_start(pcidev, BAR5_NUM);
                break;
            case 16 ... 31:
                /* bit 4-5, 2'b10: pcie to Mezz_CPLD2 */
                fpga_async_data = 0x20;
                bar_base = pci_resource_start(pcidev, BAR5_NUM);
                break;
            case 32 ... 47:
                /* bit 4-5, 2'b01: pcie to Mezz_CPLD1 */
                fpga_async_data = 0x10;
                bar_base = pci_resource_start(pcidev, BAR5_NUM);
                break;
            case 48 ... 59:
                /* bit0, 0: pcie to MB_CPLD0 */
                fpga_async_data = 0x00;
                bar_base = pci_resource_start(pcidev, BAR4_NUM);
                break;
            case 60 ... 71:
                /* bit0, 1: pcie to MB_CPLD1 */
                fpga_async_data = 0x01;
                bar_base = pci_resource_start(pcidev, BAR4_NUM);
                break;
                /* bit 4-5 2'b01: pcie to Mezz_CPLD1 */
            case 72 ... 75:
                fpga_async_data = 0x10;
                bar_base = pci_resource_start(pcidev, BAR5_NUM);
                break;
            default:
                break;
        }

        iowrite8(fpga_async_data, async_reg);

        fpga_ctl->pci_fpga_dev.fpga_i2c[i] =
            ocore_i2c_device_add((i | (port[i].mask << 8)), bar_base, port[i].offset);
        if (IS_ERR(fpga_ctl->pci_fpga_dev.fpga_i2c[i])) {
            status = PTR_ERR(fpga_ctl->pci_fpga_dev.fpga_i2c[i]);
            dev_err(dev, "rc:%d, unload Port%u[0x%ux] device\n",
                         status, i, port[i].offset);
            goto exit_ocores_device;
        }
    }
    status = sysfs_create_group(&pdev->dev.kobj, &fpga_port_stat_group);
    if (status) {
        goto exit_ocores_device;
    }

    return 0;

exit_ocores_device:
    while (i > 0) {
        i--;
        platform_device_unregister(fpga_ctl->pci_fpga_dev.fpga_i2c[i]);
    }
    release_mem_region(fpga_ctl->pci_fpga_dev.data_region5, REGION_LEN);
exit_pci_iounmap2:
    pci_iounmap(fpga_ctl->pci_fpga_dev.pci_dev, fpga_ctl->pci_fpga_dev.data_base_addr5);
exit_pci_release1:
    release_mem_region(fpga_ctl->pci_fpga_dev.data_region4, REGION_LEN);
exit_pci_iounmap1:
    pci_iounmap(fpga_ctl->pci_fpga_dev.pci_dev, fpga_ctl->pci_fpga_dev.data_base_addr4);
exit_pci_iounmap0:
    pci_iounmap(fpga_ctl->pci_fpga_dev.pci_dev, fpga_ctl->pci_fpga_dev.data_base_addr0);
exit_pci_disable:
    pci_disable_device(fpga_ctl->pci_fpga_dev.pci_dev);

    return status;
}

static int as9947_72xkb_pcie_fpga_stat_remove(struct platform_device *pdev)
{
    struct as9947_72xkb_fpga_data *fpga_ctl = platform_get_drvdata(pdev);

    if (pci_is_enabled(fpga_ctl->pci_fpga_dev.pci_dev)) {
        int i;

        sysfs_remove_group(&pdev->dev.kobj, &fpga_port_stat_group);
        /* Unregister ocore_i2c device */
        for (i = 0; i < PORT_NUM; i++) {
            platform_device_unregister(fpga_ctl->pci_fpga_dev.fpga_i2c[i]);
        }

        pci_iounmap(fpga_ctl->pci_fpga_dev.pci_dev, fpga_ctl->pci_fpga_dev.data_base_addr0);
        pci_iounmap(fpga_ctl->pci_fpga_dev.pci_dev, fpga_ctl->pci_fpga_dev.data_base_addr4);
        pci_iounmap(fpga_ctl->pci_fpga_dev.pci_dev, fpga_ctl->pci_fpga_dev.data_base_addr5);
        release_mem_region(fpga_ctl->pci_fpga_dev.data_region4, REGION_LEN);
        release_mem_region(fpga_ctl->pci_fpga_dev.data_region5, REGION_LEN);
        pci_disable_device(fpga_ctl->pci_fpga_dev.pci_dev);
    }

    return 0;
}

static struct platform_driver pcie_fpga_port_stat_driver = {
    .probe      = as9947_72xkb_pcie_fpga_stat_probe,
    .remove     = as9947_72xkb_pcie_fpga_stat_remove,
    .driver     = {
        .owner = THIS_MODULE,
        .name  = DRVNAME,
    },
};

static int __init as9947_72xkb_pcie_fpga_init(void)
{
    int status = 0;

    /*
     * Create FPGA platform driver and device
     */
    status = platform_driver_register(&pcie_fpga_port_stat_driver);
    if (status < 0) {
        return status;
    }

    pdev = platform_device_register_simple(DRVNAME, -1, NULL, 0);
    if (IS_ERR(pdev)) {
        status = PTR_ERR(pdev);
        goto exit_pci;
    }

    return status;

exit_pci:
    platform_driver_unregister(&pcie_fpga_port_stat_driver);

    return status;
}

static void __exit as9947_72xkb_pcie_fpga_exit(void)
{
    platform_device_unregister(pdev);
    platform_driver_unregister(&pcie_fpga_port_stat_driver);
}


module_init(as9947_72xkb_pcie_fpga_init);
module_exit(as9947_72xkb_pcie_fpga_exit);

MODULE_AUTHOR("Willy Liu <willy_liu@accton.com>");
MODULE_DESCRIPTION("AS9947-72XKB FPGA via PCIE");
MODULE_LICENSE("GPL");
