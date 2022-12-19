#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define LED_CTRL_DRV_NAME 	"LED_Control"
#define BUF_SIZE  		5	

static dev_t driverno;			// device number
static struct cdev *gpio_cdev; 		// character device
static struct class *gpio_class; 	// device class
static struct gpio leds[] = {		// three leds in this struct
	{ 2, GPIOF_OUT_INIT_LOW, "LED_0"}, // { the gpio number, GPIO configuration as specified by GPIOF_*, a literal description string of this GPIO}
	{ 3, GPIOF_OUT_INIT_LOW, "LED_1"},
	{ 4, GPIOF_OUT_INIT_LOW, "LED_2"},
};

// default values of three gpio
static int gpio0 = 2;
static int gpio1 = 3;
static int gpio2 = 4;

// the value of gpio can be changed when insmod
module_param(gpio0, int, S_IRUGO);	// We need module_param() to send parameter to module ( in userspace, we can use argc & argv). 
					// The first parameter is the name of parameter.
					// The second parameter is the type of parameter.
					// The third parameter is the file permission in sysfs. 
MODULE_PARM_DESC(gpio0, "GPIO-0 pin to use");
module_param(gpio1, int, S_IRUGO);
MODULE_PARM_DESC(gpio1, "GPIO-1 pin to use");
module_param(gpio2, int, S_IRUGO);
MODULE_PARM_DESC(gpio2, "GPIO-2 pin to use");

static ssize_t write_LED( struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = write_LED
};

/*
 * struct file *filp 	: device file
 * const char *buf 	: string input from user
 * size_t count		: string length of *buf
 * loff_t *f_pos	: start position (offset) of *buf
 */
static ssize_t write_LED( struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	char kbuf[BUF_SIZE];
	unsigned int len = 1, gpio_num;

	// get major and minor number
	gpio_num = iminor(filp->f_path.dentry->d_inode);
	printk(KERN_INFO LED_CTRL_DRV_NAME " GPIO: LED_%d is modified. \n", gpio_num);

	len = ( count < BUF_SIZE ) ? ( count - 1 ) : ( BUF_SIZE - 1 );
	if( copy_from_user(kbuf, buf, strlen(buf)) != 0) return -EFAULT;
	kbuf[1] = '\0';
	printk(KERN_INFO LED_CTRL_DRV_NAME " Request from user: %s\n", kbuf);

	// 0 is off; 1 is on
	if( strcmp(kbuf, "1") == 0 )
	{
		printk(KERN_INFO LED_CTRL_DRV_NAME " LED_%d switch On \n", gpio_num);
		gpio_set_value( leds[ gpio_num ].gpio, 1 );
	} else
	{
		printk(KERN_INFO LED_CTRL_DRV_NAME " LED_%d switch Off \n", gpio_num);
		gpio_set_value( leds[ gpio_num ].gpio, 0 );
	}

	msleep(100); // sleep 100 millisecond
	return count;
}

static void led_check(void)
{
	int i;

	printk(KERN_INFO LED_CTRL_DRV_NAME " %s\n", __func__);

	for( i=0; i<ARRAY_SIZE(leds); i++)
	{
		if( (i-1) >= 0 )
			gpio_set_value(leds[i].gpio, 0);
		gpio_set_value(leds[i].gpio, 1);
		msleep(500);
	}

	gpio_set_value(leds[i-1].gpio, 0);
	printk(KERN_INFO LED_CTRL_DRV_NAME " blink ended\n");
}

static int __init led_control_init(void)
{
	int i, ret;

	printk("Initializing led_comtrol module.\n");
 
	leds[0].gpio = gpio0;
	leds[1].gpio = gpio1;
	leds[2].gpio = gpio2;
	for( i=0; i<ARRAY_SIZE(leds); i++)
		printk(KERN_INFO LED_CTRL_DRV_NAME " Set %s to GPIO %i \n", leds[i].label, leds[i].gpio);

	printk(KERN_INFO LED_CTRL_DRV_NAME " gpio_request_array \n");
	ret = gpio_request_array( leds, ARRAY_SIZE( leds ));
	
	if( ret < 0 )
	{
		printk( KERN_ERR LED_CTRL_DRV_NAME " Unable to request GPIOs: %d\n", ret);
		goto exit_gpio_request;
	}

	// Get driver number
	printk(KERN_INFO LED_CTRL_DRV_NAME " alloc_chrdev_region \n");
	ret = alloc_chrdev_region( &driverno, 0, ARRAY_SIZE( leds ), LED_CTRL_DRV_NAME);

	if( ret )
	{
		printk(KERN_EMERG LED_CTRL_DRV_NAME " alloc_chrdev_region failed\n");
		goto exit_gpio_request;
	}

	printk(KERN_INFO LED_CTRL_DRV_NAME " DRIVER No. of %s is %d\n", LED_CTRL_DRV_NAME, MAJOR(driverno));
	printk(KERN_INFO LED_CTRL_DRV_NAME " cdev_alloc\n");

	gpio_cdev = cdev_alloc();

	if( gpio_cdev == NULL )
	{
		printk(KERN_EMERG LED_CTRL_DRV_NAME " Cannot alloc cdev\n");
		ret = -ENOMEM;
		goto exit_unregister_chrdev;
	}

	printk(KERN_INFO LED_CTRL_DRV_NAME " cdev_init\n");
	cdev_init( gpio_cdev, &fops);
	gpio_cdev->owner = THIS_MODULE;

	printk(KERN_INFO LED_CTRL_DRV_NAME " cdev_add\n");
	ret = cdev_add( gpio_cdev, driverno, ARRAY_SIZE(leds));
	if(ret)
	{
		printk(KERN_EMERG LED_CTRL_DRV_NAME " cdev_add failed!\n");
		goto exit_cdev;
	}

	led_check();

	printk(KERN_INFO LED_CTRL_DRV_NAME " Create class \n");
	gpio_class = class_create(THIS_MODULE, LED_CTRL_DRV_NAME);
	if( IS_ERR( gpio_class ) )
	{
		printk(KERN_ERR LED_CTRL_DRV_NAME " class_create failed\n");
		ret = PTR_ERR(gpio_class);
		goto exit_cdev;
	}

	printk(KERN_INFO LED_CTRL_DRV_NAME " Create device\n");
	for( i=0; i<ARRAY_SIZE(leds); i++)
	{
		if( device_create( gpio_class, NULL, MKDEV(MAJOR(driverno), MINOR(driverno)+i), NULL, leds[i].label ) == NULL )
		{
			printk(KERN_ERR LED_CTRL_DRV_NAME " device_create failed\n");
			ret = -1;
			goto exit_cdev;
		}
	}

	return 0;

exit_cdev:
	cdev_del(gpio_cdev);

exit_unregister_chrdev:
	unregister_chrdev_region( driverno, ARRAY_SIZE(leds));

exit_gpio_request:
	gpio_free_array(leds, ARRAY_SIZE(leds));

	return ret;
}

static void __exit led_control_exit(void)
{
	int i;

	printk("Unloading led_control module.\n");

	printk(KERN_INFO LED_CTRL_DRV_NAME " %s\n", __func__);

	// turn all off
	for( i=0; i<ARRAY_SIZE(leds); i++)
	{
		gpio_set_value(leds[i].gpio, 0);
		device_destroy(gpio_class, MKDEV(MAJOR(driverno), MINOR(driverno) + i));
	}

	class_destroy(gpio_class);
	cdev_del(gpio_cdev);
	unregister_chrdev_region(driverno, ARRAY_SIZE(leds));
	gpio_free_array(leds, ARRAY_SIZE(leds));
}

module_init(led_control_init);
module_exit(led_control_exit);

MODULE_LICENSE("GPL");
