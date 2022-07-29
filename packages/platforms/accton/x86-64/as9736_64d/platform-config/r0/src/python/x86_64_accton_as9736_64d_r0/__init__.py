from onl.platform.base import *
from onl.platform.accton import *

import commands

class OnlPlatform_x86_64_accton_as9736_64d_r0(OnlPlatformAccton,
                                              OnlPlatformPortConfig_64x400_2x10):

    PLATFORM='x86-64-accton-as9736-64d-r0'
    MODEL="AS9736-64D"
    SYS_OBJECT_ID=".9736.64"

    def baseconfig(self):
        self.insmod('optoe')
        #self.insmod('ym2651y')
        self.insmod('accton_i2c_psu')
        os.system('insmod /lib/modules/5.4.40-OpenNetworkLinux/kernel/drivers/i2c/busses/i2c-ismt.ko')

        for m in [ 'sys', 'cpld', 'fan', 'psu', 'leds', 'fpga' ]:
            self.insmod("x86-64-accton-as9736-64d-%s.ko" % m)

        ########### initialize I2C bus 0 ###########
        # initialize multiplexer (PCA9548)
        self.new_i2c_device('pca9548', 0x72, 0)
        # initiate multiplexer (PCA9548)
        self.new_i2c_devices(
            [
                # initiate multiplexer (PCA9548)
                ('pca9548', 0x71, 3),
                ('pca9548', 0x71, 5),
                ('pca9548', 0x76, 9),
                ('pca9548', 0x70, 10),
                ('pca9548', 0x70, 11),
                ('pca9548', 0x70, 12),
                ('pca9548', 0x70, 18),
                ('pca9548', 0x70, 19),
                ]
            )

        self.new_i2c_devices([
            # initialize CPLD & FPGA
            ('as9736_64d_sys_cpld', 0x60, 6),
            ('sys_fpga', 0x60, 17),
            ('as9736_64d_fan', 0x33, 25),
            ('as9736_64d_pdb_l_cpld', 0x60, 36),
            ('as9736_64d_pdb_r_cpld', 0x60, 44),
            ('scm_cpld', 0x35, 51),
            #('as9736_64d_udb_cpld1', 0x60, 60), read by pcie
            #('as9736_64d_udb_cpld2', 0x61, 61), read by pcie
            #('as9736_64d_ldb_cpld1', 0x61, 68), read by pcie
            #('as9736_64d_ldb_cpld2', 0x62, 69), read by pcie
            ])

        self.new_i2c_devices([
            # inititate TMP
            ('lm75', 0x48, 2),
            ('lm75', 0x49, 2),
            ('lm75', 0x4C, 14),
            ('lm75', 0x49, 27),
            ('lm75', 0x48, 27),
            ('lm75', 0x48, 34),
            ('lm75', 0x49, 42),
            ('lm75', 0x48, 57),
            ('lm75', 0x4C, 58),
            ('lm75', 0x4C, 65),
            ('lm75', 0x4D, 66),
         ])

        self.new_i2c_devices([
            # initiate PSU-1
            ('as9736_64d_psu1', 0x50, 33),
            ('acbel_fsh082', 0x58, 33),

            # initiate PSU-2
            ('as9736_64d_psu2', 0x51, 41),
            ('acbel_fsh082', 0x59, 41),
         ])

        '''
        # initialize QSFP port 1~8
        for port in range(1, 5):
            self.new_i2c_device('optoe1', 0x50, port+21)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+21), shell=True)

        self.new_i2c_device('optoe1', 0x50, 27)
        subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (5, 27), shell=True)
        self.new_i2c_device('optoe1', 0x50, 26)
        subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (6, 26), shell=True)
        self.new_i2c_device('optoe1', 0x50, 29)
        subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (7, 29), shell=True)
        self.new_i2c_device('optoe1', 0x50, 28)
        subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (8, 28), shell=True)

        # initialize QSFP port 9~16
        for port in range(9, 13):
            self.new_i2c_device('optoe1', 0x50, port+9)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+9), shell=True)
        for port in range(13, 17):
            self.new_i2c_device('optoe1', 0x50, port+17)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+17), shell=True)

        # initialize QSFP port 17~24
        for port in range(17, 21):
            self.new_i2c_device('optoe1', 0x50, port+17)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+17), shell=True)
        for port in range(21, 25):
            self.new_i2c_device('optoe1', 0x50, port+25)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+25), shell=True)

        # initialize QSFP port 25~32
        for port in range(25, 33):
            self.new_i2c_device('optoe1', 0x50, port+13)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port+13), shell=True)

        # initialize SFP port 33~34
        for port in range(33, 35):
            self.new_i2c_device('optoe2', 0x50, port-17)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port-17), shell=True)
        '''

        self.new_i2c_device('as973d_64d_sys', 0x51, 20)

        return True