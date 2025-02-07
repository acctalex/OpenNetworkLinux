#!/bin/bash

#common prefix
common_prefix=/sys/devices/platform/

# CPU eeprom
sys_prefix=/as7535_28xb_sys/
cpu_eeprom_sysfs="${common_prefix}${sys_prefix}eeprom"      

# BIOS flash
support_bios_flash=1
cpu_cpld_i2c_bus="0x1"
cpu_cpld_i2c_addr="0x65"
bios_flash_reg_offset="0x11"
bios_flash_bit_field_offset=


# PSU sysfs
psu_prefix=/as7535_28xb_psu/
psu1_present_sysfs="${common_prefix}${psu_prefix}psu1_present"
psu2_present_sysfs="${common_prefix}${psu_prefix}psu2_present"
psu1_power_good_sysfs="${common_prefix}${psu_prefix}psu1_power_good"
psu2_power_good_sysfs="${common_prefix}${psu_prefix}psu2_power_good"
 
# QSFP/SFP
support_sfp=1
support_qsfpdd=1
sfp_eeprom_bus_array=(0 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48)
qsfp_eeprom_bus_array=(0 23 21 24 22)

port_status_cpld_i2c_bus_addr_array=("12-0061")
sfp_port_array=(0 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28)
qsfp_port_array=(0 1 2 3 4)

# CPU temp
cpu_temp_hwmon=$(eval "ls ${common_prefix}coretemp.0/hwmon | grep hwmon")
cpu_temp_bus_id_array=("2" "4" "5" "6" "7" "8" "9")

# System led
led_prefix=/as7535_28xb_led/
sys_led_array=("led_loc" "led_diag" "led_psu1" "led_psu2" "led_fan" "led_alarm")
sys_led_sysfs=("${common_prefix}${led_prefix}led_loc" \
               "${common_prefix}${led_prefix}led_diag" \
               "${common_prefix}${led_prefix}led_psu1" \
               "${common_prefix}${led_prefix}led_psu2" \
               "${common_prefix}${led_prefix}led_fan" \
               "${common_prefix}${led_prefix}led_alarm" )

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



