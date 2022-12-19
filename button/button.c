#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/kfifo.h>
#include <linux/mutex.h>

#define BTN_DRV_NAME	"btn_drv"
#define CLASS_NAME 	"class_btn_drv"

static unsigned int gpio_btn = 17;
static unsigned int gpio_led = 27;
static unsigned int irq_num = 0;
static spinlock_t gpio_press_lock;
static ktime_t gpio_press_time;
static struct class* char_class = NULL;
static struct device* char_device = NULL;
static int major_num;
static struct kfifo gpio_press_fifo;

static DEFINE_MUTEX(gpio_monitor_mutex);

static irq_handler_t gpio_press_action(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	uint8_t value;
	unsigned int duration;
	
	spin_lock(&gpio_press_lock);
	value = gpio_get_value(gpio_btn);
	
	if(value == 0)
	{
		if( ktime_to_ms(gpio_press_time) == 0 )
			goto finished;
		
		duration = ktime_to_ms( ktime_sub(ktime_get(), gpio_press_time) );
		
		if( duration == 0 )
			goto finished;
		
		gpio_press_time = ktime_set(0, 0);
		gpio_set_value(gpio_led, 0);
		printk(KERN_INFO " GPIO_BUTTON: Detected button release, duration : %u\n", duration);
	} else
	{
		if( ktime_to_ms(gpio_press_time) > 0 )
			goto finished;
		
		gpio_set_value(gpio_led, 1);
		printk(KERN_INFO " GPIO_BUTTON: Detected button press\n");
		gpio_press_time = ktime_get();
	}
	
finished:
	spin_unlock( &gpio_press_lock );
	return (irq_handler_t) IRQ_HANDLED;
}

static int __init button_init(void)
{
	int result = 0;
	
	printk(KERN_INFO " Initializing button driver...\n");
	
	mutex_init( &gpio_monitor_mutex );
	
	// init_led START
	if( !gpio_is_valid( gpio_led ) )
	{
		printk(KERN_INFO " Invalid GPIO %d", gpio_led);
		return -ENODEV;
	}
	
	if(gpio_request( gpio_led, "LED" ) < 0) 
		return -ENODEV;
	
	gpio_direction_output( gpio_led, 0 );
	// init_led END
	
	// init_button START
	if( !gpio_is_valid( gpio_btn ) )
	{
		printk(KERN_INFO " Invalid GPIO %d", gpio_btn);
		return -ENODEV;
	}
	
	if( gpio_request( gpio_btn, "BTN" ) < 0 ) 
		return -ENODEV;
	
	gpio_direction_input( gpio_btn );
	// init_button END
	
	gpio_press_time = ktime_set(0, 0);
	
	// init_interrupt_request START
	irq_num = gpio_to_irq( gpio_btn );
	result = request_irq( irq_num, (irq_handler_t) gpio_press_action, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_press action", NULL);
	printk(KERN_INFO "Pressed interrupt request resulted in %d\n", result);
	// init_interrupt_request END
	
	if( result < 0 )
		return result;
	
	return 0;
}

static void __exit button_exit(void)
{
	printk(KERN_INFO " Unloading button\n");
	gpio_set_value(gpio_led, 0);
	free_irq( irq_num, NULL);
	gpio_free( gpio_led );
	gpio_free( gpio_btn );
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");

















