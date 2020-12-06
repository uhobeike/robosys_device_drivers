#!/bin/bash -vx

sudo insmod i2c-lcd.ko

sudo chmod 666 /sys/bus/i2c/devices/i2c-1/new_device
sudo echo "i2c_lcd 0x3e" > /sys/bus/i2c/devices/i2c-1/new_device

sudo rmmod i2c-lcd
sudo insmod i2c-lcd.ko

sudo ln -nfs /sys/bus/i2c/devices/1-003e/lcd_row1 /dev/lcd_row_10
sudo ln -nfs /sys/bus/i2c/devices/1-003e/lcd_row2 /dev/lcd0_row_20
sudo ln -nfs /sys/bus/i2c/devices/1-003e/lcd_clear /dev/lcd_clear0

sudo chmod 666 /dev/lcd_row_10
sudo chmod 666 /dev/lcd0_row_20
sudo chmod 666 /dev/lcd_clear0

echo > /dev/lcd_clear0
echo -n "install" > /dev/lcd_row_10
echo -n "OK!!!!!" > /dev/lcd0_row_20
