from onl.platform.base import *
from onl.platform.accton import *

import commands


def eeprom_check():
    cmd = "i2cget -y 0 0x57"
    status, output = commands.getstatusoutput(cmd)
    return status

#IR3570A chip casue problem when read eeprom by i2c-block mode.
#It happen when read 16th-byte offset that value is 0x8. So disable chip 
def disable_i2c_ir3570a(addr):
    cmd = "i2cset -y 0 0x%x 0xE5 0x01" % addr
    status, output = commands.getstatusoutput(cmd)
    cmd = "i2cset -y 0 0x%x 0x12 0x02" % addr
    status, output = commands.getstatusoutput(cmd)
    return status

def ir3570_check():
    cmd = "i2cdump -y 0 0x42 s 0x9a"
    try:
        status, output = commands.getstatusoutput(cmd)
        lines = output.split('\n')
        hn = re.findall(r'\w+', lines[-1])
        version = int(hn[1], 16)
        if version == 0x24:  #Find IR3570A
            ret = disable_i2c_ir3570a(4)
        else:
            ret = 0
    except Exception as e:
        print "Error on ir3570_check() e:" + str(e)
        return -1
    return ret

class OnlPlatform_x86_64_accton_es7636bt4_r0(OnlPlatformAccton,
                                              OnlPlatformPortConfig_8x400_28x100):

    PLATFORM='x86-64-accton-es7636bt4-r0'
    MODEL="IXR7220-D4"
    SYS_OBJECT_ID=".7220.D4"

    def baseconfig(self):
        self.insmod('optoe')
        self.insmod('accton_i2c_psu')
        for m in [ 'cpld', 'fan', 'psu', 'leds' ]:
            self.insmod("x86-64-accton-es7636bt4-%s.ko" % m)

        ########### initialize I2C bus 0 ###########
        # initialize multiplexer (PCA9548)        
        self.new_i2c_device('pca9548', 0x77, 0)
        # initiate multiplexer (PCA9548)
        self.new_i2c_devices(
            [
                # initiate multiplexer (PCA9548)                
                ('pca9548', 0x71, 2),
                ('pca9548', 0x72, 2),
                ('pca9548', 0x73, 20),
                ('pca9548', 0x73, 21),
                ('pca9548', 0x73, 22),
                ('pca9548', 0x73, 23),
                ('pca9548', 0x73, 24)
            ]
        )

        self.new_i2c_devices([
            # initialize CPLD
             #initiate CPLD
            ('es7636bt4_cpld1', 0x60, 10),            
            ('es7636bt4_cpld2', 0x62, 10),
            ('es7636bt4_cpld3', 0x64, 10),
            ])
        self.new_i2c_devices([
            # initiate fan
              # initiate chassis fan
            ('es7636bt4_fan', 0x66, 14),

            # inititate LM75           
            ('lm75', 0x48, 15),
            ('lm75', 0x49, 15),
            ('lm75', 0x4a, 15),
            ('lm75', 0x4c, 15),
            ('lm75', 0x4b, 15),
         ])

        self.new_i2c_devices([
            # initiate PSU-1
            ('es7636bt4_psu1', 0x51, 9),
            ('acbel_fsh082', 0x59, 9),           

            # initiate PSU-2
            ('es7636bt4_psu2', 0x50, 9),
            ('acbel_fsh082', 0x58, 9),
         ])

        # initialize QSFP port 1~28. QSFP-DD port 29~36
        for port in range(1, 37):
            if port <= 28 :
                self.new_i2c_device('optoe1', 0x50, port+24)
                subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+24), shell=True)
            else:
                self.new_i2c_device('optoe2', 0x50, port+28)
                subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+28), shell=True)

        #Dut to new board eeprom i2c-addr is 0x57, old board eeprom i2c-addr is 0x56. So need to check and set correct i2c-addr sysfs
        ret=eeprom_check()
        if ret==0:
            self.new_i2c_device('24c02', 0x57, 0)
        else:
            ir3570_check()
            self.new_i2c_device('24c02', 0x56, 0)
      
        return True
