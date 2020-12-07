#include <linux/cdev.h>
#include <linux/delay.h> 
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/i2c.h> 
#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h> 
#include <linux/spi/spi.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include "i2c-lcd.h"

MODULE_AUTHOR("Tatsuhiro Ikebe");
MODULE_DESCRIPTION("driver for 3_LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.3.3");

#define DRIVER_NAME "3_LED"

#define NUM_DEV_TOTAL   7
#define NUM_DEV_LCD     1 //(3)
#define NUM_DEV_ADC     1
#define NUM_DEV_LED     3

#define DEV_MAJOR 0
#define DEV_MINOR 0

#define DEVNAME_LCD_10      "lcd_row1"
#define DEVNAME_LCD_20      "lcd_row2"
#define DEVNAME_LCD_CLEAR   "lcd_clear"
#define DEVNAME_ADC         "analog_read"
#define DEVNAME_LED         "led_blink"

static int _major_lcd_row_10 = DEV_MAJOR;
static int _minor_lcd_row_10 = DEV_MINOR;
static int _major_lcd_row_20 = DEV_MAJOR;
static int _minor_lcd_row_20 = DEV_MINOR;
static int _major_lcd_clear = DEV_MAJOR;
static int _minor_lcd_clear = DEV_MINOR;
static int _major_adc = DEV_MAJOR;
static int _minor_adc = DEV_MINOR;
static int _major_led_blink = DEV_MAJOR;
static int _minor_led_blink = DEV_MINOR;

static dev_t dev;
static struct cdev *cdev_array = NULL;
static struct class *class_lcd_row_10 = NULL;
static struct class *class_lcd_row_20 = NULL;
static struct class *class_lcd_clear = NULL;
static struct class *class_adc = NULL;
static struct class *class_led_blink = NULL;

static volatile int cdev_index = 0;

#define LCD_I2C_ADDR    0x3e
#define LCD_OSC_FREQ    0x04
#define LCD_AMP_RATIO   0x02
#define LCD_COLS    8

#define MCP320X_PACKET_SIZE	3
#define MCP320X_DIFF		0
#define MCP320X_SINGLE		1
#define MCP3204_CHANNELS	4

#define MAX_BUFLEN 64

#define RPI_GPF_OUTPUT 0x1

int ch_show_data = 0;

static volatile u32 *gpio_base = NULL;
#define LED0_BASE 20
#define LED1_BASE 21
#define LED2_BASE 26


struct i2c_lcd_device {
    struct i2c_client *client;
};

static int contrast = 56;

static const struct i2c_device_id i2c_lcd_id[] = {
    { "i2c_lcd", 0 },{}
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

static int lcd_row1_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
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
static DEVICE_ATTR(lcd_row1, S_IWUSR|S_IWGRP , NULL, lcd_row1_store );

static int lcd_row2_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
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
static DEVICE_ATTR(lcd_row2, S_IWUSR|S_IWGRP , NULL, lcd_row2_store );

static int lcd_clear_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int size = strlen( buf );
    struct i2c_lcd_device *data = (struct i2c_lcd_device *)dev_get_drvdata(dev);
    
    i2c_lcd_cleardisplay(data->client);
    
    return size;
}
static DEVICE_ATTR(lcd_clear, S_IWUSR|S_IWGRP , NULL, lcd_clear_store );

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

    printk(KERN_INFO "2");
    i2c_lcd_cleardisplay(client);
    i2c_lcd_setcursor(client, 0,0);
    i2c_lcd_puts(client, "Hello,123");
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
    .address_list = i2c_lcd_addr,
    .driver   = {
        .owner = THIS_MODULE,
        .name = "i2c_lcd",
    },
};

static struct spi_board_info mcp3204_info = {
	.modalias = "mcp3204",
	.max_speed_hz = 1000000,
	.bus_num = 0,
	.chip_select = 0,
	.mode = SPI_MODE_3,
};

struct mcp3204_drvdata {
	struct spi_device *spi;
	struct mutex lock;
	unsigned char tx[MCP320X_PACKET_SIZE]  ____cacheline_aligned;
	unsigned char rx[MCP320X_PACKET_SIZE]  ____cacheline_aligned;
	struct spi_transfer xfer ____cacheline_aligned;
	struct spi_message msg ____cacheline_aligned;
};

/* パラメータでバス番号とCSを指定できるようにする */
static int spi_bus_num     = 0;
static int spi_chip_select = 0;
module_param( spi_bus_num, int, S_IRUSR | S_IRGRP | S_IROTH |  S_IWUSR );
module_param( spi_chip_select, int, S_IRUSR | S_IRGRP | S_IROTH |  S_IWUSR );

static unsigned int mcp3204_get_value(int channel)
{

	struct device *dev;
	struct mcp3204_drvdata *data;
	struct spi_device *spi;
	char str[128];
	struct spi_master *master;

	unsigned int r = 0;
	unsigned char c = channel & 0x03;

	master = spi_busnum_to_master(mcp3204_info.bus_num);
	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev),
		 mcp3204_info.chip_select);

	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	spi = to_spi_device(dev);
	data = (struct mcp3204_drvdata *)spi_get_drvdata(spi);
	
	mutex_lock( &data->lock );
	data->tx[0] = 1 << 2;		// スタートビット
	data->tx[0] |= 1 << 1;		// Single
	data->tx[1] = c << 6;		// チャンネル
	data->tx[2] = 0;
	
	if( spi_sync( data->spi, &data->msg) ) { 
		printk(KERN_INFO "spi_sync_transfer returned non zero\n" );
	}
	mutex_unlock(&data->lock);
	
	r =  (data->rx[1] & 0x0F) << 8;
	r |= data->rx[2];
	
	return r;
}

static int mcp3204_probe(struct spi_device *spi)
{
	struct mcp3204_drvdata *data;
	
	printk(KERN_INFO "mcp3204 probe\n");
	
	/* SPIを設定する */
	spi->max_speed_hz = mcp3204_info.max_speed_hz;
	spi->mode = mcp3204_info.mode;
	spi->bits_per_word = 8;
	if( spi_setup( spi ) ) {
		printk(KERN_ERR "spi_setup returned error\n");
		return -ENODEV;
	}
	
	data = kzalloc( sizeof(struct mcp3204_drvdata), GFP_KERNEL );
	if(data == NULL ) {
		printk(KERN_ERR "%s: no memory\n", __func__ );
		return -ENODEV;
	}
	data->spi = spi;
	mutex_init( &data->lock );
	
	data->xfer.tx_buf = data->tx;
	data->xfer.rx_buf = data->rx;
	data->xfer.bits_per_word = 8;
	data->xfer.len = MCP320X_PACKET_SIZE;
	data->xfer.cs_change = 0;
	data->xfer.delay_usecs = 0;
	data->xfer.speed_hz = 1000000;
	spi_message_init_with_transfers( &data->msg, &data->xfer, 1 );
	
	spi_set_drvdata( spi, data );

	return 0;
}

static int mcp3204_remove(struct spi_device *spi)
{
	struct mcp3204_drvdata *data;
	data = (struct mcp3204_drvdata *)spi_get_drvdata(spi);

	kfree(data);
	printk(KERN_INFO "mcp3204 removed\n");
	
	return 0;
}

static struct spi_device_id mcp3204_id[] = {
	{ "mcp3204", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, mcp3204_id);

static struct spi_driver mcp3204_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table = mcp3204_id,
	.probe	= mcp3204_probe,
	.remove	= mcp3204_remove,
};


static void spi_remove_device(struct spi_master *master, unsigned int cs)
{
   struct device *dev;
   char str[128];

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);
	// SPIデバイスを探す
	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	// あったら削除
	if( dev ){
		printk(KERN_INFO "Delete %s\n", str);
		device_del(dev);
	}
}

static int rpi_gpio_function_set(int pin, uint32_t func)
{
	int index = pin / 10;
	uint32_t mask = ~(0x7 << ((pin % 10) * 3));

	gpio_base[index] = (gpio_base[index] & mask) | ((func & 0x7) << ((pin % 10) * 3));

	return 1;
}

static int led_blink_on(int ledno)
{
	switch (ledno) {
    case 0:
        gpio_base[7] = (1 << LED0_BASE);
        break;
    case 1:
        gpio_base[7] = (1 << LED1_BASE);
        break;
    case 2:
        gpio_base[7] = (1 << LED2_BASE);
        break;
    }


	return 0;
}

static int led_blink_off(int ledno)
{
	switch (ledno) {
	case 0:
		gpio_base[10] = (1 << LED0_BASE);
		break;
	case 1:
		gpio_base[10] = (1 << LED1_BASE);
		break;
	case 2:
		gpio_base[10] = (1 << LED2_BASE);
		break;
    }

	return 0;
}

static int dev_open(struct inode *inode, struct file *filp)
{
	int *minor = (int *)kmalloc(sizeof(int), GFP_KERNEL);
	int major = MAJOR(inode->i_rdev);
	*minor = MINOR(inode->i_rdev);

	filp->private_data = (void *)minor;

	return 0;
}

static ssize_t analog_read(struct file* filp, char* buf, size_t count, loff_t* pos)
{
	unsigned char rw_buf[MAX_BUFLEN];

	snprintf(rw_buf, sizeof(rw_buf), "%d\n", mcp3204_get_value(0));
	
	if(copy_to_user((void *)buf, &rw_buf, strlen(rw_buf))){
		return -EFAULT;
	}

	*pos += strlen(rw_buf);

	return strlen(rw_buf);
}

static ssize_t led_blink(struct file* flip, const char* buf, size_t count, loff_t* pos)
{
	char cval;
	int ret;
	int minor = *((int *)flip->private_data);
    
	if (count > 0) {
		if (copy_from_user(&cval, buf, sizeof(char))) {
			return -EFAULT;
		}
		switch (cval) {
		case '1':
			ret = led_blink_on(minor);
			break;
		case '0':
			ret = led_blink_off(minor);
			break;
		}
		return sizeof(char);
	}

	return 0;
}

static int init_mcp(void)
{
	struct spi_master *master;
	struct spi_device *spi_device;
	
	spi_register_driver(&mcp3204_driver);
	
	mcp3204_info.bus_num = spi_bus_num;
	mcp3204_info.chip_select = spi_chip_select;
	master = spi_busnum_to_master(mcp3204_info.bus_num);
	if( ! master ) {
		printk( KERN_ERR "spi_busnum_to_master returned NULL\n");
		spi_unregister_driver(&mcp3204_driver);
		return -ENODEV;
	}
	
	/* 初期状態でspidev0.0が専有しているので必ず取り除く */
	spi_remove_device(master, mcp3204_info.chip_select);
	
	spi_device = spi_new_device( master, &mcp3204_info );
	if( !spi_device ) {
		printk(KERN_ERR "spi_new_device returned NULL\n" );
		spi_unregister_driver(&mcp3204_driver);
		return -ENODEV;
	}

    return 0;
}

static void exit_mcp(void)
{
	struct spi_master *master;

	master = spi_busnum_to_master(mcp3204_info.bus_num);
	if( master ) {
		spi_remove_device(master, mcp3204_info.chip_select );
	}
	else {
		printk( KERN_INFO "mcp3204 remove error\n");
	}
	spi_unregister_driver(&mcp3204_driver);
}

static int init_led(void)
{
	gpio_base = ioremap_nocache(0x3f200000, 0xA0);
    /* GPIO Init */
	rpi_gpio_function_set(LED0_BASE, RPI_GPF_OUTPUT);
	rpi_gpio_function_set(LED1_BASE, RPI_GPF_OUTPUT);
	rpi_gpio_function_set(LED2_BASE, RPI_GPF_OUTPUT);
}

static struct file_operations lcd_fops = {
    .owner = THIS_MODULE
};

static struct file_operations adc_fops = {
	.read = analog_read
};

static struct file_operations led_blink_fops = {
    .open = dev_open, .write = led_blink
};

static int lcd_write_10_register_dev(void)
{
    int retval;
    dev_t dev;
    dev_t devno;

    /* 空いているメジャー番号を使ってメジャー
        &マイナー番号をカーネルに登録する */
    retval = alloc_chrdev_region(&dev, /* 結果を格納するdev_t構造体 */
                     DEV_MINOR, /* ベースマイナー番号 */
                     NUM_DEV_LCD, /* デバイスの数 */
                     DEVNAME_LCD_10 /* デバイスドライバの名前 */
                     );

    if (retval < 0) {
        printk(KERN_ERR "alloc_chrdev_region failed.\n");
        return retval;
    }
    _major_lcd_row_10 = MAJOR(dev);

    /* デバイスクラスを作成する */
    class_lcd_row_10 = class_create(THIS_MODULE, DEVNAME_LCD_10);
    if (IS_ERR(class_lcd_row_10)) {
        return PTR_ERR(class_lcd_row_10);
    }

    devno = MKDEV(_major_lcd_row_10, _minor_lcd_row_10);
    /* キャラクタデバイスとしてこのモジュールをカーネルに登録する */
    cdev_init(&(cdev_array[cdev_index]), &lcd_fops);
    cdev_array[cdev_index].owner = THIS_MODULE;
    if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
        /* 登録に失敗した */
        printk(KERN_ERR "cdev_add failed minor = %d\n", _minor_lcd_row_10);
    } else {
        /* デバイスノードの作成 */
        device_create(class_lcd_row_10, NULL, devno, NULL, DEVNAME_LCD_10 "%u", _minor_lcd_row_10);
    }

    cdev_index++;

    return 0;
}

static int lcd_write_20_register_dev(void)
{
    int retval;
    dev_t dev;
    dev_t devno;

    /* 空いているメジャー番号を使ってメジャー
        &マイナー番号をカーネルに登録する */
    retval = alloc_chrdev_region(&dev, /* 結果を格納するdev_t構造体 */
                     DEV_MINOR, /* ベースマイナー番号 */
                     NUM_DEV_LCD, /* デバイスの数 */
                     DEVNAME_LCD_20 /* デバイスドライバの名前 */
                     );

    if (retval < 0) {
        printk(KERN_ERR "alloc_chrdev_region failed.\n");
        return retval;
    }
    _major_lcd_row_20 = MAJOR(dev);

    /* デバイスクラスを作成する */
    class_lcd_row_20 = class_create(THIS_MODULE, DEVNAME_LCD_20);
    if (IS_ERR(class_lcd_row_20)) {
        return PTR_ERR(class_lcd_row_20);
    }

    devno = MKDEV(_major_lcd_row_20, _minor_lcd_row_20);
    /* キャラクタデバイスとしてこのモジュールをカーネルに登録する */
    cdev_init(&(cdev_array[cdev_index]), &lcd_fops);
    cdev_array[cdev_index].owner = THIS_MODULE;
    if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
        /* 登録に失敗した */
        printk(KERN_ERR "cdev_add failed minor = %d\n", _minor_lcd_row_20);
    } else {
        /* デバイスノードの作成 */
        device_create(class_lcd_row_20, NULL, devno, NULL, DEVNAME_LCD_20 "%u", _minor_lcd_row_20);
    }

    cdev_index++;

    return 0;
}

static int lcd_clear_register_dev(void)
{
    int retval;
    dev_t dev;
    dev_t devno;

    /* 空いているメジャー番号を使ってメジャー
        &マイナー番号をカーネルに登録する */
    retval = alloc_chrdev_region(&dev, /* 結果を格納するdev_t構造体 */
                     DEV_MINOR, /* ベースマイナー番号 */
                     NUM_DEV_LCD, /* デバイスの数 */
                     DEVNAME_LCD_CLEAR /* デバイスドライバの名前 */
                     );

    if (retval < 0) {
        printk(KERN_ERR "alloc_chrdev_region failed.\n");
        return retval;
    }
    _major_lcd_clear = MAJOR(dev);

    /* デバイスクラスを作成する */
    class_lcd_clear = class_create(THIS_MODULE, DEVNAME_LCD_CLEAR);
    if (IS_ERR(class_lcd_clear)) {
        return PTR_ERR(class_lcd_clear);
    }

    devno = MKDEV(_major_lcd_clear, _minor_lcd_clear);
    /* キャラクタデバイスとしてこのモジュールをカーネルに登録する */
    cdev_init(&(cdev_array[cdev_index]), &lcd_fops);
    cdev_array[cdev_index].owner = THIS_MODULE;
    if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
        /* 登録に失敗した */
        printk(KERN_ERR "cdev_add failed minor = %d\n", _minor_lcd_clear);
    } else {
        /* デバイスノードの作成 */
        device_create(class_lcd_clear, NULL, devno, NULL, DEVNAME_LCD_CLEAR "%u", _minor_lcd_clear);
    }

    cdev_index++;

    return 0;
}

static int adc_register_dev(void)
{
    int retval;
    dev_t dev;
    dev_t devno;

    /* 空いているメジャー番号を使ってメジャー
        &マイナー番号をカーネルに登録する */
    retval = alloc_chrdev_region(&dev, /* 結果を格納するdev_t構造体 */
                     DEV_MINOR, /* ベースマイナー番号 */
                     NUM_DEV_ADC, /* デバイスの数 */
                     DEVNAME_ADC /* デバイスドライバの名前 */
                     );

    if (retval < 0) {
        printk(KERN_ERR "alloc_chrdev_region failed.\n");
        return retval;
    }
    _major_adc = MAJOR(dev);

    /* デバイスクラスを作成する */
    class_adc = class_create(THIS_MODULE, DEVNAME_ADC);
    if (IS_ERR(class_adc)) {
        return PTR_ERR(class_adc);
    }

    devno = MKDEV(_major_adc, _minor_adc);
    /* キャラクタデバイスとしてこのモジュールをカーネルに登録する */
    cdev_init(&(cdev_array[cdev_index]), &adc_fops);
    cdev_array[cdev_index].owner = THIS_MODULE;
    if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
        /* 登録に失敗した */
        printk(KERN_ERR "cdev_add failed minor = %d\n", _minor_adc);
    } else {
        /* デバイスノードの作成 */
        device_create(class_adc, NULL, devno, NULL, DEVNAME_ADC "%u", _minor_adc);
    }

    cdev_index++;

    return 0;
}

static int led_register_dev(void)
{
	int retval;
	dev_t dev;
	dev_t devno;
    int i;

	/* 空いているメジャー番号を使ってメジャー&
	   マイナー番号をカーネルに登録する */
	retval = alloc_chrdev_region(&dev, /* 結果を格納するdev_t構造体 */
				     DEV_MINOR, /* ベースマイナー番号 */
				     NUM_DEV_LED, /* デバイスの数 */
				     DEVNAME_LED /* デバイスドライバの名前 */
				     );

	if (retval < 0) {
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
	_major_led_blink = MAJOR(dev);

	/* デバイスクラスを作成する */
	class_led_blink = class_create(THIS_MODULE, DEVNAME_LED);
	if (IS_ERR(class_led_blink)) {
		return PTR_ERR(class_led_blink);
	}

	/* デバイスの数だけキャラクタデバイスを登録する */
	for (i = 0; i < NUM_DEV_LED; i++) {
		devno = MKDEV(_major_led_blink, _minor_led_blink + i);
		/* キャラクタデバイスとしてこのモジュールをカーネルに登録する */
		cdev_init(&(cdev_array[cdev_index]), &led_blink_fops);
		cdev_array[cdev_index].owner = THIS_MODULE;

		if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
			/* 登録に失敗した */
			printk(KERN_ERR "cdev_add failed minor = %d\n",
			       _minor_led_blink + i);
		} else {
			/* デバイスノードの作成 */
			device_create(class_led_blink, NULL, devno, NULL,
				      DEVNAME_LED "%u", _minor_led_blink + i);
		}
		cdev_index++;
	}

	return 0;
}

static int init_mod(void)
{
    i2c_add_driver(&i2c_lcd_driver);
    init_mcp();
	init_led();

    int retval;
    size_t size;

    size = sizeof(struct cdev) * NUM_DEV_TOTAL;
    cdev_array = (struct cdev *)kmalloc(size, GFP_KERNEL);
    
    /* /dev/lcd_row_10*/
    retval = lcd_write_10_register_dev();
    if (retval != 0) {
        printk(KERN_ALERT "%s: lcd register failed.\n",
               DRIVER_NAME);
        return retval;
    }

    /* /dev/lcd_row_20*/
    retval = lcd_write_20_register_dev();
    if (retval != 0) {
        printk(KERN_ALERT "%s: lcd register failed.\n",
               DRIVER_NAME);
        return retval;
    }

    /* /dev/lcd_clear0*/
    retval = lcd_clear_register_dev();
    if (retval != 0) {
        printk(KERN_ALERT "%s: lcd register failed.\n",
               DRIVER_NAME);
        return retval;
    }

    /* /dev/analog_read0*/
    retval = adc_register_dev();
    if (retval != 0) {
        printk(KERN_ALERT "%s: lcd register failed.\n",
               DRIVER_NAME);
        return retval;
    }

    /* /dev/led_blink0 ~ 2 [0: 20][1: 21][2: 26]*/
	retval = led_register_dev();
    if (retval != 0) {
        printk(KERN_ALERT "%s: lcd register failed.\n",
               DRIVER_NAME);
        return retval;
    }

    printk(KERN_INFO "%s is loaded. major:%d\n",__FILE__,MAJOR(dev));

    return 0;
}

static void cleanup_mod(void)
{
    exit_mcp();
    i2c_del_driver(&i2c_lcd_driver);

    int i;
    dev_t devno;
    dev_t devno_top;

    /* --- remove char device --- */
    for (i = 0; i < NUM_DEV_TOTAL; i++) {
        cdev_del(&(cdev_array[i]));
    }

    /* /dev/lcd_row_10*/
    devno = MKDEV(_major_lcd_row_10, _minor_lcd_row_10);
    device_destroy(class_lcd_row_10, devno);
    unregister_chrdev_region(devno, NUM_DEV_LCD);

    /* /dev/lcd_row_20*/
    devno = MKDEV(_major_lcd_row_20, _minor_lcd_row_20);
    device_destroy(class_lcd_row_20, devno);
    unregister_chrdev_region(devno, NUM_DEV_LCD);

    /* /dev/lcd_clear0*/
    devno = MKDEV(_major_lcd_clear, _minor_lcd_clear);
    device_destroy(class_lcd_clear, devno);
    unregister_chrdev_region(devno, NUM_DEV_LCD);

    /* /dev/analog_read0*/
    devno = MKDEV(_major_adc, _minor_adc);
    device_destroy(class_adc, devno);
    unregister_chrdev_region(devno, NUM_DEV_ADC);

	/* /dev/led_blink0 ~ 2 [0: 20][1: 21][2: 26]*/
    devno_top = MKDEV(_major_led_blink, _minor_led_blink);
	for (i = 0; i < NUM_DEV_LED; i++) {
		devno = MKDEV(_major_led_blink, _minor_led_blink + i);
		device_destroy(class_led_blink, devno);
	}
    unregister_chrdev_region(devno, NUM_DEV_LED);

    /* --- remove device node --- */
    class_destroy(class_lcd_row_10);
    class_destroy(class_lcd_row_20);
    class_destroy(class_lcd_clear);
    class_destroy(class_adc);
	class_destroy(class_led_blink);

    kfree(cdev_array);
}

module_init(init_mod);
module_exit(cleanup_mod);