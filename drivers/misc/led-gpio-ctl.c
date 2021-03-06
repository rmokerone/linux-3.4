#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <mach/sys_config.h>
#include <mach/platform.h>
#include <linux/timer.h>
#include <linux/sched.h>

u32 led_hdl[1];
char *main_key = "led_para";
char *led_key[1] = {
	"led0_gpio",
};

struct timer_list led_timer;
static bool led_status;

static int sunxi_gpio_req(struct gpio_config *gpio)
{
	int            ret = 0;
	char           pin_name[8] = {0};
	unsigned long  config;

	sunxi_gpio_to_name(gpio->gpio, pin_name);
	config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, gpio->mul_sel);
	ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
	if (ret) {
	        printk("set gpio %s mulsel failed.\n",pin_name);
	        return -1;
        }

	if (gpio->pull != GPIO_PULL_DEFAULT){
                config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, gpio->pull);
                ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
                if (ret) {
                        printk("set gpio %s pull mode failed.\n",pin_name);
                        return -1;
                }
	}

	if (gpio->drv_level != GPIO_DRVLVL_DEFAULT){
                config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, gpio->drv_level);
                ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
                if (ret) {
                        printk("set gpio %s driver level failed.\n",pin_name);
                        return -1;
                }
        }

	if (gpio->data != GPIO_DATA_DEFAULT) {
                config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, gpio->data);
                ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
                if (ret) {
                        printk("set gpio %s initial val failed.\n",pin_name);
                        return -1;
                }
        }

	return 0;
}

void set_gpio_status(int handler, bool status)
{
	gpio_set_value(handler, status);
}

static void set_led_status(unsigned long arg)
{
	int i;

	led_status = led_status ? false : true;
	
	for (i = 0; i < 1; i++)
		set_gpio_status(led_hdl[i], led_status);

	mod_timer(&led_timer, jiffies + HZ);
}

static int __init led_ctl_probe(struct platform_device *pdev)
{
	script_item_u val;
	script_item_value_type_e type;
	struct gpio_config  *gpio_p = NULL;
	int i;

	for (i = 0; i < 1; i++) {
		type = script_get_item(main_key, led_key[i], &val);
		if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
			printk("fetch the [%s] %s failed!\n", main_key, led_key[i]);
		}
		gpio_p = &val.gpio;
		led_hdl[i] = gpio_p->gpio;
		if (led_hdl[i] == 0) {
			printk("gpio led_hdl[%d] failed.\n", i);
		}
		sunxi_gpio_req(gpio_p);
	}
	printk("led control probe success!\n");

	return 0;
}

static int __exit led_ctl_remove(struct platform_device *pdev)
{
	del_timer(&led_timer);

	return 0;
}

static struct platform_driver led_ctl_driver = {
	.driver		= {
		.name	= "led_ctl",
		.owner	= THIS_MODULE,
	},
	.remove		= __exit_p(led_ctl_remove),
};

static int __init led_ctl_init(void)
{
	init_timer(&led_timer);
	led_timer.expires = jiffies + HZ;
	led_timer.data = 100;
	led_timer.function = set_led_status;
	add_timer(&led_timer);

	return platform_driver_probe(&led_ctl_driver, led_ctl_probe);
}

static void __exit led_ctl_exit(void)
{
	kfree(&led_timer);
	platform_driver_unregister(&led_ctl_driver);
}

module_init(led_ctl_init);
module_exit(led_ctl_exit);

MODULE_AUTHOR("Link Gou <link.gou@gmail.com>");
MODULE_DESCRIPTION("SW power gpio control driver");
MODULE_LICENSE("GPL");
