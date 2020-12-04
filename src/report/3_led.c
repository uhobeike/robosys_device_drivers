#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

MODULE_AUTHOR("Tatsuhiro Ikebe");
MODULE_DESCRIPTION("driver for 3_LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;

#define DRIVER_NAME	"mcp3204"

#define MCP320X_PACKET_SIZE	3
#define MCP320X_DIFF		0
#define MCP320X_SINGLE		1
#define MCP3204_CHANNELS	4

#define MAX_BUFLEN 64

int ch_show_data = 0;

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


static unsigned int mcp3204_get_value( int channel )
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
	int ret;
	
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

static ssize_t analog_read(struct file* filp, char* buf, size_t count, loff_t* pos)
{
	unsigned char rw_buf[MAX_BUFLEN];

	snprintf(rw_buf, sizeof(rw_buf), "%d\n", mcp3204_get_value(0));
	
	printk(KERN_INFO "%d",mcp3204_get_value(0));

	if(copy_to_user((void *)buf, &rw_buf, strlen(rw_buf))){
		return -EFAULT;
	}

	*pos += strlen(rw_buf);

	return strlen(rw_buf);
}

static struct file_operations analog_sensor_fops = {
	.owner = THIS_MODULE,
	.read = analog_read
};

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

static int init_mod(void)
{
	init_mcp();

	int retval;

	retval = alloc_chrdev_region(&dev, 0 , 1, "analog_sensor_data");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}

	cdev_init(&cdv, &analog_sensor_fops);
	retval = cdev_add(&cdv, dev, 1);
	if(retval < 0){
		printk(KERN_ERR "cdev_add failed. major:%d, minor:%d",MAJOR(dev),MINOR(dev));
		return retval;
	}

	cls = class_create(THIS_MODULE, "analog_sensor");
	if(IS_ERR(cls)){
		printk(KERN_ERR"class_create failed.");
		return PTR_ERR(cls);
	}

	device_create(cls, NULL, dev, NULL , "analog_sensor%d", MINOR(dev));
	
	printk(KERN_INFO "%s is loaded. major:%d\n",__FILE__,MAJOR(dev));
	
	return 0;
}

static void cleanup_mod(void)
{
	exit_mcp();
	cdev_del(&cdv);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "%s is uniloaded. major:%d\n",__FILE__,MAJOR(dev));
}

module_init(init_mod);
module_exit(cleanup_mod);


