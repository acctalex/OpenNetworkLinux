#!/bin/bash

# CPU eeprom
cpu_eeprom_bus_id="0"      
cpu_eeprom_i2c_addr="57"

if [ -f "/sys/bus/i2c/devices/${cpu_eeprom_bus_id}-0056/eeprom" ]; then
    cpu_eeprom_i2c_addr="56"
fi

cpu_eeprom_sysfs="/sys/bus/i2c/devices/${cpu_eeprom_bus_id}-00${cpu_eeprom_i2c_addr}/eeprom"

# BIOS flash
cpu_cpld_i2c_bus="0x0"
cpu_cpld_i2c_addr="0x65"
bios_flash_reg_offset="0x2"
bios_flash_bit_field_offset=4

# PSU sysfs
psu1_present_sysfs="/sys/bus/i2c/devices/10-0051/psu_present"
psu2_present_sysfs="/sys/bus/i2c/devices/9-0050/psu_present"
psu1_power_good_sysfs="/sys/bus/i2c/devices/10-0051/psu_power_good"
psu2_power_good_sysfs="/sys/bus/i2c/devices/9-0050/psu_power_good"

# QSFP/SFP
sfp_eeprom_bus_array=(-1 57 58)
qsfp_eeprom_bus_array=(0  25 26 27 28 29 30 31 31 33 34 \
                       35 36 37 38 39 40 -1 41 42 43 44 \
                       45 46 47 48 49 50 51 52 53 54 55 \
                       56)

port_status_cpld_i2c_bus_addr_array=("20-0061" "21-0062")
sfp_port_array=(-1 33 34)
qsfp_port_array=(0 1  2  3  4  5  6  7  8  9  10 \
                11 12 13 14 15 16 -1 17 18 19 20 \
                21 22 23 24 25 26 27 28 29 30 31 \
                32)

# CPU temp
cpu_temp_hwmon=$(eval "ls /sys/devices/platform/coretemp.0/hwmon | grep hwmon")
cpu_temp_bus_id_array=("1" "2" "3" "4" "5")

# System led
sys_led_path_prefix="accton_as9716_32d_led"
sys_led_array=("diag" "loc" "fan" "psu1" "psu2")

sys_led_array=("diag" "loc" "fan" "psu1" "psu2")
sys_led_sysfs=("/sys/class/leds/accton_as9716_32d_led::diag/brightness" \
               "/sys/class/leds/accton_as9716_32d_led::loc/brightness" \
               "/sys/class/leds/accton_as9716_32d_led::fan/brightness" \
               "/sys/class/leds/accton_as9716_32d_led::psu1/brightness" \
               "/sys/class/leds/accton_as9716_32d_led::psu2/brightness")

sys_beacon_led_sysfs="/sys/class/leds/accton_as9716_32d_led::loc/brightness"

# USB
usb_auth_file_array=("/sys/bus/usb/devices/usb1/authorized" \
                     "/sys/bus/usb/devices/usb1/authorized_default" \
                     "/sys/bus/usb/devices/1-0:1.0/authorized" \
                     "/sys/bus/usb/devices/1-1/authorized" \
                     "/sys/bus/usb/devices/1-1:1.0/authorized" \
                     "/sys/bus/usb/devices/usb2/authorized" \
                     "/sys/bus/usb/devices/usb2/authorized_default" \
                     "/sys/bus/usb/devices/2-1/authorized" \
                     "/sys/bus/usb/devices/2-0:1.0/authorized" \
                     "/sys/bus/usb/devices/2-1:1.0/authorized" \
                     "/sys/bus/usb/devices/usb3/authorized" \
                     "/sys/bus/usb/devices/usb3/authorized_default" \
                     "/sys/bus/usb/devices/3-0:1.0/authorized")
