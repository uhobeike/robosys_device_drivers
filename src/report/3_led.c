#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Tatsuhiro Ikebe");
MODULE_DESCRIPTION("driver for 3_LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.1");

#define DRIVER_NAME	"mcp3204"

#define MCP320X_PACKET_SIZE	3
#define MCP320X_DIFF		0
#define MCP320X_SINGLE		1
#define MCP3204_CHANNELS	4

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


static unsigned int mcp3204_get_value( struct mcp3204_drvdata *data, int channel )
{
	unsigned int r = 0;
	unsigned char c = channel & 0x03;
	
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

static int ch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mcp3204_drvdata *data = (struct mcp3204_drvdata *)dev_get_drvdata(dev);
	int c = attr->attr.name[2] - 0x30;
	int v = 0;
	
	if( (c > -1) && (c < MCP3204_CHANNELS) ) {
		v = mcp3204_get_value(data, c);
	}
	return snprintf (buf, PAGE_SIZE, "%d\n", v);
}
static DEVICE_ATTR(ch0, S_IRUSR|S_IRGRP|S_IROTH, ch_show,NULL );
static DEVICE_ATTR(ch1, S_IRUSR|S_IRGRP|S_IROTH, ch_show,NULL );
static DEVICE_ATTR(ch2, S_IRUSR|S_IRGRP|S_IROTH, ch_show,NULL );
static DEVICE_ATTR(ch3, S_IRUSR|S_IRGRP|S_IROTH, ch_show,NULL );

static struct spi_board_info mcp3204_info = {
	.modalias = "mcp3204",
	.max_speed_hz = 1000000,
	.bus_num = 0,
	.chip_select = 0,
	.mode = SPI_MODE_3,
};

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
	
	ret = device_create_file( &spi->dev, &dev_attr_ch0 );
	if(ret) {
		printk(KERN_ERR "failed to add ch0 attribute\n" );
	}	
	ret = device_create_file( &spi->dev, &dev_attr_ch1 );
	if(ret) {
		printk(KERN_ERR "failed to add ch1 attribute\n" );
	}	
	ret = device_create_file( &spi->dev, &dev_attr_ch2 );
	if(ret) {
		printk(KERN_ERR "failed to add ch2 attribute\n" );
	}	
	ret = device_create_file( &spi->dev, &dev_attr_ch3 );
	if(ret) {
		printk(KERN_ERR "failed to add ch3 attribute\n" );
	}	

	return 0;
}

static int mcp3204_remove(struct spi_device *spi)
{
	struct mcp3204_drvdata *data;
	data = (struct mcp3204_drvdata *)spi_get_drvdata(spi);
	
	device_remove_file( &spi->dev, &dev_attr_ch0 );
	device_remove_file( &spi->dev, &dev_attr_ch1 );
	device_remove_file( &spi->dev, &dev_attr_ch2 );
	device_remove_file( &spi->dev, &dev_attr_ch3 );
	
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

static int init_mod(void)
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

static void exit_mod(void)
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
module_init(init_mod);
module_exit(exit_mod);


