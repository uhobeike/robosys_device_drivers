#include <linux/delay.h> 
#include <linux/device.h>
#include <linux/kernel.h> 
#include <linux/mutex.h> 
#include <linux/module.h> 
#include <linux/slab.h> 
#include <linux/i2c.h> 
#include <linux/string.h>
#include "i2c_lcd.h"

#define LCD_I2C_ADDR	0x3e
#define LCD_OSC_FREQ	0x04
#define LCD_AMP_RATIO	0x02
#define LCD_COLS	8

struct i2c_lcd_device {
        struct i2c_client *client;
};

static int contrast = 56;

static const struct i2c_device_id i2c_lcd_id[] = {
        { "i2c_lcd", 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, i2c_lcd_id);

static const unsigned short i2c_lcd_addr[] = { LCD_I2C_ADDR, I2C_CLIENT_END };

static int i2c_lcd_cleardisplay(struct i2c_client *client)
{
	i2c_smbus_write_byte_data( client, LCD_RS_CMD_WRITE, LCD_CLEARDISPLAY );
	usleep_range(1080, 2000);
	
	return 0;
}

static int i2c_lcd_puts(struct i2c_client *client, char *str )
{
	i2c_smbus_write_i2c_block_data( client, LCD_RS_DATA_WRITE, strlen(str), (unsigned char *)str );
	usleep_range(26,100);
	
	return 0;
}

static int i2c_lcd_setcursor(struct i2c_client *client, int col, int row )
{
	int row_offs[] = { 0x00, 0x40, 0x14, 0x54 };
	
	i2c_smbus_write_byte_data( client, LCD_RS_CMD_WRITE, LCD_SETDDRAMADDR | (col + row_offs[row]) );
	usleep_range(26,100);
	
	return 0;
}

static int lcd_row1_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	char str[LCD_COLS+1];
	int size = strlen( buf );
	struct i2c_lcd_device *data = (struct i2c_lcd_device *)dev_get_drvdata(dev);
	
	i2c_lcd_setcursor(data->client, 0, 0 );
	strncpy(str, buf, LCD_COLS );
	str[LCD_COLS] = '0';
	
	i2c_lcd_puts(data->client, str);
	
	return size;
}
static DEVICE_ATTR(lcd_row1, S_IWUSR|S_IWGRP|S_IWOTH , NULL, lcd_row1_store );

static int lcd_row2_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	char str[LCD_COLS+1];
	int size = strlen( buf );
	struct i2c_lcd_device *data = (struct i2c_lcd_device *)dev_get_drvdata(dev);
	
	i2c_lcd_setcursor(data->client, 0, 1 );
	strncpy( str, buf, LCD_COLS );
	str[LCD_COLS] = '0';
	
	i2c_lcd_puts(data->client, str);
	
	return size;
}
static DEVICE_ATTR(lcd_row2, S_IWUSR|S_IWGRP|S_IWOTH , NULL, lcd_row2_store );

static int lcd_clear_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int size = strlen( buf );
	struct i2c_lcd_device *data = (struct i2c_lcd_device *)dev_get_drvdata(dev);
	
	i2c_lcd_cleardisplay(data->client);
	
	return size;
}
static DEVICE_ATTR(lcd_clear, S_IWUSR|S_IWGRP|S_IWOTH, NULL, lcd_clear_store );

static int i2c_lcd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_lcd_device *dev;
	int ret;
	unsigned char data[8];
	
	printk( KERN_INFO "probing... addr=%d\n", client->addr);

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK)) {
		printk(KERN_ERR "%s: needed i2c functionality is not supported\n", __func__);
		return -ENODEV;
	}
	
	/* LCDの初期化 */
	ret = i2c_smbus_write_byte_data( client, LCD_RS_CMD_WRITE, LCD_FUNCTIONSET | LCD_FUNC_8BITMODE | LCD_FUNC_2LINE);
	if( ret < 0 ) {
		printk(KERN_ERR "%s: Could not write first function set to i2c lcd device\n", __func__);
		return -ENODEV;
	}
	usleep_range(27, 100);
	i2c_smbus_write_byte_data( client, LCD_RS_CMD_WRITE, LCD_FUNCTIONSET | LCD_FUNC_8BITMODE | LCD_FUNC_2LINE | LCD_FUNC_INSTABLE );
	usleep_range(27, 100);
	
	data[0] = LCD_IS_OSC | LCD_OSC_FREQ;
	data[1] = LCD_IS_CONTSET1 | contrast_lower(contrast);
	data[2] = LCD_IS_CONTSET2 | contrast_upper(contrast);
	data[3] = LCD_IS_FOLLOWER | LCD_IS_FOLLOWER_ON | LCD_AMP_RATIO;
	
	ret = i2c_smbus_write_i2c_block_data( client, LCD_RS_CMD_WRITE, 4, data );
	if( ret < 0 ) {
		printk(KERN_ERR "%s: Could not initialize i2c lcd device\n", __func__);
		return -ENODEV;
	}
	msleep(200);
	
	i2c_smbus_write_byte_data( client, LCD_RS_CMD_WRITE, LCD_DISPLAYCONTROL | LCD_DISP_ON );
	usleep_range(27, 100);
	i2c_smbus_write_byte_data( client, LCD_RS_CMD_WRITE, LCD_ENTRYMODESET | LCD_ENTRYLEFT);
	usleep_range(27, 100);

	i2c_lcd_cleardisplay(client);
	i2c_lcd_setcursor(client, 0,0);
	i2c_lcd_puts(client, "Hello,");
	i2c_lcd_setcursor(client, 0,1);
	i2c_lcd_puts(client, "RasPi");
	
	dev = kzalloc(sizeof(struct i2c_lcd_device), GFP_KERNEL);
	if (dev == NULL) {
		printk(KERN_ERR "%s: no memory\n", __func__);
		return -ENOMEM;
	}

	dev->client = client;
	i2c_set_clientdata(client, dev);
	
	ret = device_create_file( &client->dev, &dev_attr_lcd_row1 );
	if(ret) {
		printk(KERN_ERR "failed to add lcd_row1 attribute\n" );
	}
	
	ret = device_create_file( &client->dev, &dev_attr_lcd_row2 );
	if(ret) {
		printk(KERN_ERR "failed to add lcd_row2 attribute\n" );
	}
	
	ret = device_create_file( &client->dev, &dev_attr_lcd_clear );
	if(ret) {
		printk(KERN_ERR "failed to add lcd_clear attribute\n" );
	}
	
	return 0;
}

static int i2c_lcd_remove(struct i2c_client *client)
{
	struct i2c_lcd_device *dev;

	printk( KERN_INFO "removing ... \n" );
	
	device_remove_file( &client->dev,  &dev_attr_lcd_row1);
	device_remove_file( &client->dev,  &dev_attr_lcd_row2);
	device_remove_file( &client->dev,  &dev_attr_lcd_clear);
	
	dev = (struct i2c_lcd_device *)i2c_get_clientdata(client);
	kfree(dev);
	return 0;
}

#define i2c_lcd_suspend NULL
#define i2c_lcd_resume  NULL

static struct i2c_driver i2c_lcd_driver = {
	.probe    = i2c_lcd_probe,
	.remove   = i2c_lcd_remove,
	.id_table = i2c_lcd_id,
	.suspend  = i2c_lcd_suspend,
	.resume   = i2c_lcd_resume,
	.address_list = i2c_lcd_addr,
	.driver   = {
		.owner = THIS_MODULE,
		.name = "i2c_lcd",
	},
};

module_i2c_driver(i2c_lcd_driver);

MODULE_DESCRIPTION("I2C LCD driver");
MODULE_LICENSE("GPL");

