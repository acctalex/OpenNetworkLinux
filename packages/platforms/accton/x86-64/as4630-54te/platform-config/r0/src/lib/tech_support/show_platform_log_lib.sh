#!/bin/bash

# CPU eeprom
cpu_eeprom_bus_id="1"      
cpu_eeprom_i2c_addr="57"

cpu_eeprom_sysfs="/sys/bus/i2c/devices/${cpu_eeprom_bus_id}-00${cpu_eeprom_i2c_addr}/eeprom"


# BIOS flash
support_bios_flash=1
cpu_cpld_i2c_bus="0x1"
cpu_cpld_i2c_addr="0x65"
bios_flash_reg_offset="0x11"
bios_flash_bit_field_offset=2

# PSU sysfs
psu1_present_sysfs="/sys/bus/i2c/devices/10-0050/psu_present"
psu2_present_sysfs="/sys/bus/i2c/devices/11-0051/psu_present"
psu1_power_good_sysfs="/sys/bus/i2c/devices/10-0050/psu_power_good"
psu2_power_good_sysfs="/sys/bus/i2c/devices/11-0051/psu_power_good"
 
# QSFP/SFP
support_sfp=1
support_qsfpdd=1
sfp_eeprom_bus_array=(0 18 19 20 21)
qsfp_eeprom_bus_array=(0 22 23)

port_status_cpld_i2c_bus_addr_array=("3-0060")
sfp_port_array=(0 49 50 51 52)
qsfp_port_array=(0 53 54)

# CPU temp
cpu_temp_hwmon=$(eval "ls /sys/devices/platform/coretemp.0/hwmon | grep hwmon")
cpu_temp_bus_id_array=("1" "4" "8" "10" "14")

# System led
sys_led_array=("diag" "poe" "fan" "pri" "psu1" "psu2" "stk1" "stk2")
sys_led_sysfs=("/sys/class/leds/as4630_54te::diag/brightness" \
               "/sys/class/leds/as4630_54te::poe/brightness" \
               "/sys/class/leds/as4630_54te::fan/brightness" \
               "/sys/class/leds/as4630_54te::pri/brightness" \
               "/sys/class/leds/as4630_54te::psu1/brightness" \
               "/sys/class/leds/as4630_54te::psu2/brightness" \
               "/sys/class/leds/as4630_54te::stk1/brightness" \
               "/sys/class/leds/as4630_54te::stk2/brightness")

sys_beacon_led_sysfs=""

# USB
usb_auth_file_array=("/sys/bus/usb/devices/usb1/authorized" \
                     "/sys/bus/usb/devices/usb1/authorized_default" \
                     "/sys/bus/usb/devices/1-0:1.0/authorized" \
                     "/sys/bus/usb/devices/1-2/authorized" \
                     "/sys/bus/usb/devices/1-2:1.0/authorized" \
                     "/sys/bus/usb/devices/usb2/authorized" \
                     "/sys/bus/usb/devices/usb2/authorized_default" \
                     "/sys/bus/usb/devices/2-0:1.0/authorized")

