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
psu1_present_sysfs="/sys/bus/i2c/devices/17-0051/psu_present"
psu2_present_sysfs="/sys/bus/i2c/devices/13-0053/psu_present"
psu1_power_good_sysfs="/sys/bus/i2c/devices/17-0051/psu_power_good"
psu2_power_good_sysfs="/sys/bus/i2c/devices/13-0053/psu_power_good"

# QSFP/SFP
port_status_cpld_i2c_bus_addr_array=("12-0062" "18-0060")
sfp_eeprom_bus_array=(0 42 41 44 43 47 45 46 50 48 \
                     49 52 51 53 56 55 54 58 57 60 \
                     59 61 63 62 64 66 68 65 67 69 \
                     71 -1 72 70 74 73 76 75 77 79 \
                     78 80 81 82 84 85 83 87 88 86 \
                     22 23)

qsfp_eeprom_bus_array=(-1 25 26 27 28 29 30 31 32)

sfp_port_array=(0 1  2  3  4  5  6  7  8  9  \
               10 11 12 13 14 15 16 17 18 19 \
               20 21 22 23 24 25 26 27 28 29 \
               30 -1 31 32 33 34 35 36 37 38 \
               39 40 41 42 43 44 46 46 47 48 \
               57 58)

qsfp_port_array=(-1 49 50 51 52 53 54 55 56)

# CPU temp
cpu_temp_hwmon=$(eval "ls /sys/devices/platform/coretemp.0/hwmon | grep hwmon")
cpu_temp_bus_id_array=("1" "2" "3" "4" "5")

# System led
sys_led_array=("diag" "loc" "fan" "psu1" "psu2")
sys_led_sysfs=("/sys/class/leds/accton_as7326_56x_led::diag/brightness" \
               "/sys/class/leds/accton_as7326_56x_led::loc/brightness" \
               "/sys/class/leds/accton_as7326_56x_led::fan/brightness" \
               "/sys/class/leds/accton_as7326_56x_led::psu1/brightness" \
               "/sys/class/leds/accton_as7326_56x_led::psu2/brightness")

sys_beacon_led_sysfs="/sys/class/leds/accton_as7326_56x_led::loc/brightness"

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



