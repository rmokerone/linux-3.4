/*
 * hctusbtouchscreen.c
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

//#define DEBUG

#define DRIVER_VERSION		"v0.6"
#define DRIVER_AUTHOR		"Daniel Ritz <daniel.ritz@gmx.ch>"
#define DRIVER_DESC		"USB Touchscreen Driver"

/* device specifc data/functions */
struct usbtouch_usb;
struct usbtouch_device_info {
	int min_xc, max_xc;
	int min_yc, max_yc;
	int min_press, max_press;
	int rept_size;

	/*
	 * Always service the USB devices irq not just when the input device is
	 * open. This is useful when devices have a watchdog which prevents us
	 * from periodically polling the device. Leave this unset unless your
	 * touchscreen device requires it, as it does consume more of the USB
	 * bandwidth.
	 */
	bool irq_always;

	void (*process_pkt) (struct usbtouch_usb *usbtouch, unsigned char *pkt, int len);

	/*
	 * used to get the packet len. possible return values:
	 * > 0: packet len
	 * = 0: skip one byte
	 * < 0: -return value more bytes needed
	 */
	int  (*get_pkt_len) (unsigned char *pkt, int len);

	int  (*read_data)   (struct usbtouch_usb *usbtouch, unsigned char *pkt);
	int  (*init)        (struct usbtouch_usb *usbtouch);
	void (*exit)	    (struct usbtouch_usb *usbtouch);
};

/* a usbtouch device */
struct usbtouch_usb {
	unsigned char *data;
	dma_addr_t data_dma;
	unsigned char *buffer;
	int buf_len;
	struct urb *irq;
	struct usb_interface *interface;
	struct input_dev *input;
	struct usbtouch_device_info *type;
	char name[128];
	char phys[64];
	void *priv;

	int x, y;
	int touch, press;
};

/* device types */
enum {
	DEVTYPE_IGNORE = -1,
 	DEVTYPE_HC_OPTICAL, //鸿诚电子光学触摸
	DEVTYPE_HC_IR_MULTI_small,  //鸿诚电子红外触摸小屏
	DEVTYPE_HC_IR_MULTI_large,	//鸿诚电子红外触摸大屏
};

/***************
    	鸿诚电子红外触摸小屏
**************/

#define HC_IR_MULTI_small_DEVICE(vend, prod) {					\
        .match_flags = (USB_DEVICE_ID_MATCH_DEVICE |		\
                        USB_DEVICE_ID_MATCH_INT_CLASS |		\
                        USB_DEVICE_ID_MATCH_INT_PROTOCOL),	\
        .idVendor = (vend),			\
        .idProduct = (prod),					\
        .bInterfaceClass = USB_INTERFACE_CLASS_HID,		\
        .bInterfaceProtocol = 0,	\
        .driver_info = DEVTYPE_HC_IR_MULTI_small		\
}

/*******************************
    	光学双点触摸屏
  ******************************/
#define HC_OPTICAL_MULTI_DEVICE(vend, prod) {					\
        .match_flags = (USB_DEVICE_ID_MATCH_DEVICE |		\
                        USB_DEVICE_ID_MATCH_INT_CLASS |		\
                        USB_DEVICE_ID_MATCH_INT_PROTOCOL),	\
        .idVendor = (vend),			\
        .idProduct = (prod),					\
        .bInterfaceClass = USB_INTERFACE_CLASS_HID,		\
        .bInterfaceProtocol = 0,	\
        .driver_info = DEVTYPE_HC_OPTICAL		\
}

/********************************
*	鸿诚电子红外触摸大屏
*
*********************************/
#define HC_IR_MULTI_large_DEVICE(vend, prod) {					\
        .match_flags = (USB_DEVICE_ID_MATCH_DEVICE |		\
                        USB_DEVICE_ID_MATCH_INT_CLASS |		\
                        USB_DEVICE_ID_MATCH_INT_PROTOCOL),	\
        .idVendor = (vend),			\
        .idProduct = (prod),					\
        .bInterfaceClass = USB_INTERFACE_CLASS_HID,		\
        .bInterfaceProtocol = 0,	\
        .driver_info = DEVTYPE_HC_IR_MULTI_large		\
}

static const struct usb_device_id usbtouch_devices[] = {
#ifdef 	CONFIG_TOUCHSCREEN_USB_HCT_IR_small
		HC_IR_MULTI_small_DEVICE(0x265f,0x0005),	  
#endif	
#ifdef 	CONFIG_TOUCHSCREEN_USB_HCT_OPTICAL		
		HC_OPTICAL_MULTI_DEVICE(0x265f,0x0003),
#endif
#ifdef  CONFIG_TOUCHSCREEN_USB_HCT_IR_large		
		HC_IR_MULTI_large_DEVICE(0x1ff7,0x0013),
#endif
{}
};


// X轴反向
//#define X_AX_REVERSE
// Y轴反向
//#define Y_AX_REVERSE


// 鸿诚电子红外光学触摸
static int hc_ir_optical_multi_init(struct usbtouch_usb *usbtouch)
{
#ifdef DEBUG
	printk(" \n hc_ir_optical_multi_init       \n");
#endif

	int ret = -ENOMEM;
	struct usb_device *dev = interface_to_usbdev(usbtouch->interface);
	unsigned char *buf;
	
	buf = kmalloc(16, GFP_KERNEL);
	if (!buf)
		goto err_nobuf;
	// 读取当前触摸配置状态
	buf[0] = buf[1] = 0xFF;
	ret = usb_control_msg(dev, usb_rcvctrlpipe (dev, 0),
						  0x01,
						  USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						  0x0308, 0, buf, 2, USB_CTRL_SET_TIMEOUT);
	if (ret < 0){
		printk(" error ret = %d \n",ret );
		goto err_out;
	}
	// 配置为多点触摸模式
	buf[0]=0x07;
	buf[1]=0x02;
	buf[2]=0xe2;
	ret = usb_control_msg(dev, usb_sndctrlpipe (dev, 0),
						  0x09,   
						  USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						  0x0307, 0, buf, 3, USB_CTRL_SET_TIMEOUT);
	if (ret < 0){
		printk(" error ret = %d \n",ret );
		goto err_out;
	}
	// 读取当前配置
	buf[0] = buf[1] = 0xFF;
	ret = usb_control_msg(dev, usb_rcvctrlpipe (dev, 0),
						  0x01,
						  USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						  0x0308, 0, buf, 2, USB_CTRL_SET_TIMEOUT);
	if(ret > 0)
	return 0;
err_out:
	printk("error ret = %d \n",ret );
	kfree(buf);
err_nobuf:
	return 1;
}

static int hc_ir_optical_multi_read_data(struct usbtouch_usb *dev, unsigned char *pkt)
{
#ifdef DEBUG
		printk("Hc Ir Read Data %02x %02x %02x %02x ,%02x %02x %02x %02x, %02x %02x %02x %02x ,%02x %02x \n", pkt[0], pkt[1], pkt[2], pkt[3],
       	pkt[4], pkt[5], pkt[6], pkt[7], pkt[8], pkt[9], pkt[10],
        pkt[11], pkt[12], pkt[13]);
#endif

       if(pkt[0] == 0x01)
      {
           int x=0,y=0;
         if(pkt[13] == 0x02)
         {
             x= (pkt[4] << 8) | pkt[3];
             y= (pkt[6] << 8) | pkt[5];

#ifdef       X_AX_REVERSE
						 x = 4095-x;
#endif
#ifdef       Y_AX_REVERSE
						 y = 4095-y;
#endif
             x = (x*(dev->type->max_xc))/4095;
             y = (y*(dev->type->max_yc))/4095;

#ifdef	DEBUG
             printk("%s(%d) : hc_ir_optical_multi_read_data : src x %d  y %d  \n", __FILE__, __LINE__, x,y);
#endif

			 if(pkt[1] == 0x3)
             {
                input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 255);//finger size
                input_report_key(dev->input, BTN_TOUCH, 1);
             }
             else
             {
                input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 0);//finger size
                input_report_key(dev->input, BTN_TOUCH, 0);
             }
             input_report_abs(dev->input, ABS_MT_POSITION_X, x);
             input_report_abs(dev->input, ABS_MT_POSITION_Y, y);

             input_mt_sync(dev->input);

             x= (pkt[10] << 8) | pkt[9];
             y= (pkt[12] << 8) | pkt[11];


#ifdef       X_AX_REVERSE
						 x = 4095-x;
#endif
#ifdef       Y_AX_REVERSE
						 y = 4095-y;
#endif

             x = (x*(dev->type->max_xc))/4095;
             y = (y*(dev->type->max_yc))/4095;

#ifdef	DEBUG
             printk("%s(%d) : hc_ir_optical_multi_read_data : src x %d  y %d  \n", __FILE__, __LINE__, x,y);
#endif

			 if(pkt[7] == 0x3)
             {
                input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 255);//finger size
                input_report_key(dev->input, BTN_TOUCH, 1);
             }
             else
             {
                input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 0);//finger size
                input_report_key(dev->input, BTN_TOUCH, 0);
             }
             input_report_abs(dev->input, ABS_MT_POSITION_X, x);
             input_report_abs(dev->input, ABS_MT_POSITION_Y, y);
             input_mt_sync(dev->input);
             input_sync(dev->input);

         }
         else if(pkt[13] == 0x01)
         {
             x= (pkt[4] << 8) | pkt[3];
             y= (pkt[6] << 8) | pkt[5];

#ifdef       X_AX_REVERSE
						 x = 4095-x;
#endif
#ifdef       Y_AX_REVERSE
						 y = 4095-y;
#endif
             x = (x*(dev->type->max_xc))/4095;
             y = (y*(dev->type->max_yc))/4095;

#ifdef	DEBUG
             printk("%s(%d) : hc_ir_optical_multi_read_data : src x %d  y %d  \n", __FILE__, __LINE__, x,y);
#endif

			 if(pkt[1] == 0x03)
             {
                input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 255);//finger size
                input_report_key(dev->input, BTN_TOUCH, 1);
             }
             else
             {
                input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 0);//finger size
                input_report_key(dev->input, BTN_TOUCH, 0);
             }
             input_report_abs(dev->input, ABS_MT_POSITION_X, x);
             input_report_abs(dev->input, ABS_MT_POSITION_Y, y);

             input_mt_sync(dev->input);
             input_sync(dev->input);
         }
         return 0;
     }
       else
     {
#ifdef	DEBUG
        printk("***************************do nothing**********************************\n");
#endif
	   }

     return 1;
}


// 鸿诚电子红外触摸大屏
static int hc_ir_multi_large_init(struct usbtouch_usb *usbtouch)
{
#ifdef DEBUG
	printk(" \n hc_ir_multi_large_init		 \n");
#endif

	int ret = -ENOMEM;
	struct usb_device *dev = interface_to_usbdev(usbtouch->interface);
	unsigned char *buf;
	
	buf = kmalloc(16, GFP_KERNEL);
	if (!buf)
		goto err_nobuf;
	// 读取当前触摸配置状态
	buf[0] = buf[1] = 0xFF;
	ret = usb_control_msg(dev, usb_rcvctrlpipe (dev, 0),
						  0x01,
						  USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						  0x0308, 0, buf, 2, USB_CTRL_SET_TIMEOUT);
	if (ret < 0){
		printk(" error ret = %d \n",ret );
		goto err_out;
	}

	// 配置为多点触摸模式
	buf[0]=0x04;
	buf[1]=0x02;
	buf[2]=0x7d;
	ret = usb_control_msg(dev, usb_sndctrlpipe (dev, 0),
						  0x09,   
						  USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						  0x0307, 0, buf, 3, USB_CTRL_SET_TIMEOUT);
	if (ret < 0){
		printk(" error ret = %d \n",ret );
		goto err_out;
	}

	// 读取当前配置
	buf[0] = buf[1] = 0xFF;
	ret = usb_control_msg(dev, usb_rcvctrlpipe (dev, 0),
						  0x01,
						  USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						  0x0308, 0, buf, 2, USB_CTRL_SET_TIMEOUT);

	if(ret > 0)
	   return 0;
err_out:
	printk("error ret = %d \n",ret );
	kfree(buf);
err_nobuf:
	return 1;
}

static int hc_ir_multi_large_read_data(struct usbtouch_usb *dev, unsigned char *pkt)
{	
	int i;

#ifdef DEBUG
	printk("hc_ir_multi_large_read_data:\n");
	printk("ReportID:%02x \n",pkt[0]);
	for(i=1; i <= pkt[61]*6; i++)
	{
		printk("%02x ",pkt[i]);
		if(i%6 == 0)
			printk("\n");
	}
	printk("ActualCount:%02x \n",pkt[61]);
#endif

	/* 10点触摸*/
	if(pkt[0] == 0x02)
	{
		int j;
		int k=1;
		int x=0;
		int y=0;
		
		for(j=0; j < pkt[61]; j++)
			{	
				if(pkt[k]&0x03 == 0x03)
				{
					input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 255);//finger size
					input_report_key(dev->input, BTN_TOUCH, 1);
				}
				else
				{
					input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 0);//finger size
					input_report_key(dev->input, BTN_TOUCH, 0);
				}
				k += 2; 
				x= (pkt[k+1] << 8) | pkt[k];
				k += 2;
				y= (pkt[k+1] << 8) | pkt[k];
				k += 2;

#ifdef       X_AX_REVERSE
				x = 4095-x;
#endif
#ifdef       Y_AX_REVERSE
				y = 4095-y;
#endif
				x = (x*(dev->type->max_xc))/4095;
				y = (y*(dev->type->max_yc))/4095;
				
				input_report_abs(dev->input, ABS_MT_POSITION_X, x);
				input_report_abs(dev->input, ABS_MT_POSITION_Y, y);
				input_mt_sync(dev->input);

#ifdef	DEBUG				
				printk("%s(%d) : hc_ir_multi_large_read_data : src[%d] x %d  y %d  \n", __FILE__, __LINE__, j,x,y);
#endif
			}
		
		input_sync(dev->input);

		
		return 0;			
	}
	else
	{
#ifdef	DEBUG
		printk("***************************do nothing**********************************\n");
#endif
	}

	 return 1;
}


static struct usbtouch_device_info usbtouch_dev_info[] = {

/****************HC IR small touchscreen *********************************************/

#ifdef 	CONFIG_TOUCHSCREEN_USB_HCT_IR_small
		[DEVTYPE_HC_IR_MULTI_small] = {
  			.min_xc		= 0x0,
 			.max_xc		= 1920-1,
  			.min_yc		= 0x0,
  			.max_yc		= 1080-1,
  			.rept_size	= 14,
  			.read_data	= hc_ir_optical_multi_read_data,
  			.init 	    =  hc_ir_optical_multi_init,
 		},	
#endif

/****************HC Optical touchscreen *********************************************/

#ifdef 	CONFIG_TOUCHSCREEN_USB_HCT_OPTICAL
		[DEVTYPE_HC_OPTICAL]={
  			.min_xc		= 0x0,
  			.max_xc		= 1920-1,
  			.min_yc		= 0x0,
  			.max_yc		= 1080-1,
  			.rept_size	= 14, 
  			.read_data	= hc_ir_optical_multi_read_data,
  			.init 	    =  hc_ir_optical_multi_init,
		},
#endif

/*****************HC IR large touchscreen*************************************************************/

#ifdef 	CONFIG_TOUCHSCREEN_USB_HCT_IR_large
		[DEVTYPE_HC_IR_MULTI_large]={
  			.min_xc		= 0x0,
  			.max_xc		= 1920-1,
  			.min_yc		= 0x0,
  			.max_yc		= 1080-1,
  			.rept_size	= 62, 
  			.read_data	= hc_ir_multi_large_read_data,
  			.init 	    =  hc_ir_multi_large_init,
		}
#endif

};

static int usbtouch_open(struct input_dev *input)
{
	struct usbtouch_usb *usbtouch = input_get_drvdata(input);

	usbtouch->irq->dev = interface_to_usbdev(usbtouch->interface);

	if (!usbtouch->type->irq_always) {
		if (usb_submit_urb(usbtouch->irq, GFP_KERNEL))
		  return -EIO;
	}

	return 0;
}

static void usbtouch_close(struct input_dev *input)
{
	struct usbtouch_usb *usbtouch = input_get_drvdata(input);

	if (!usbtouch->type->irq_always)
		usb_kill_urb(usbtouch->irq);
}

static void usbtouch_irq(struct urb *urb)
{
#ifdef	DEBUG
    printk("--------------------------------------------usbtouch_irq----------------------------------\n");
#endif

	struct usbtouch_usb *usbtouch = urb->context;
	int retval;

	switch (urb->status) {
	case 0:
		/* success */
#ifdef	DEBUG
        printk("------usbtouch_irq:success------\n");
#endif
		break;
	case -ETIME:
		/* this urb is timing out */
#ifdef	DEBUG		
        printk("------usbtouch_irq:this urb is timing out------\n");
#endif
		dbg("%s - urb timed out - was the device unplugged?",
		    __func__);
		return;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
	case -EPIPE:
		/* this urb is terminated, clean up */
#ifdef	DEBUG		
        printk("------usbtouch_irq:this urb is terminated, clean up------\n");
#endif
		dbg("%s - urb shutting down with status: %d",
		    __func__, urb->status);
		return;
	default:
#ifdef	DEBUG		
        printk("------usbtouch_irq:this is default process------\n");
#endif
		dbg("%s - nonzero urb status received: %d",
		    __func__, urb->status);
		goto exit;
	}

	usbtouch->type->read_data(usbtouch, usbtouch->data);

exit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		err("%s - usb_submit_urb failed with result: %d",
		    __func__, retval);
#ifdef	DEBUG			
        printk("------usbtouch_irq:this is end ,exit------\n");
#endif

}

static void usbtouch_free_buffers(struct usb_device *udev,
				  struct usbtouch_usb *usbtouch)
{
	usb_free_coherent(udev, usbtouch->type->rept_size,
			  usbtouch->data, usbtouch->data_dma);
	kfree(usbtouch->buffer);
}


static struct usb_endpoint_descriptor *
usbtouch_get_input_endpoint(struct usb_host_interface *interface)
{
	int i;

	for (i = 0; i < interface->desc.bNumEndpoints; i++)
		if (usb_endpoint_dir_in(&interface->endpoint[i].desc))
			return &interface->endpoint[i].desc;

	return NULL;
}

static int usbtouch_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
#ifdef	DEBUG			
    printk("--------------------------------------usbtouch_probe----------------------------------1\n");
#endif

	struct usbtouch_usb *usbtouch;
	struct input_dev *input_dev;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usbtouch_device_info *type;
	int err = -ENOMEM;

#if 1 //Add judge for Single-point and multi-point touchscreen
       bool multi_point=false;
       if((0x265f == udev->descriptor.idVendor && 0x0003 == udev->descriptor.idProduct) ||
	   	  (0x1ff7 == udev->descriptor.idVendor && 0x0013 == udev->descriptor.idProduct) ||
          (0x265f == udev->descriptor.idVendor && 0x0005 == udev->descriptor.idProduct))
	   {
#ifdef	DEBUG			
            printk("-------------------------------------\ntouchscreen is multi-point\n-------------------------------\n");
#endif
			multi_point=true;
       }
#endif

	/* some devices are ignored */
	if (id->driver_info == DEVTYPE_IGNORE)
		return -ENODEV;

	endpoint = usbtouch_get_input_endpoint(intf->cur_altsetting);
	if (!endpoint)
		return -ENXIO;

	usbtouch = kzalloc(sizeof(struct usbtouch_usb), GFP_KERNEL);

	input_dev = input_allocate_device();
	if (!usbtouch || !input_dev)
		goto out_free;

	type = &usbtouch_dev_info[id->driver_info];
	usbtouch->type = type;

	usbtouch->data = usb_alloc_coherent(udev, type->rept_size,
					    GFP_KERNEL, &usbtouch->data_dma);
	if (!usbtouch->data)
       {
#ifdef	DEBUG
        printk("--------------------------------------------usbtouch_probe----------------------------------2\n");
#endif
		goto out_free;
       }
	
	if (type->get_pkt_len) {
			usbtouch->buffer = kmalloc(type->rept_size, GFP_KERNEL);
			if (!usbtouch->buffer)
			{
#ifdef	DEBUG
				printk("--------------------------------------------usbtouch_probe----------------------------------3\n");
#endif
				goto out_free_buffers;
			}
	}

	usbtouch->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbtouch->irq) {
		dbg("%s - usb_alloc_urb failed: usbtouch->irq", __func__);
		goto out_free_buffers;
	}

	
	usbtouch->interface = intf;
	usbtouch->input = input_dev;

	if (udev->manufacturer)
		strlcpy(usbtouch->name, udev->manufacturer, sizeof(usbtouch->name));

	if (udev->product) {
		if (udev->manufacturer)
			strlcat(usbtouch->name, " ", sizeof(usbtouch->name));
		strlcat(usbtouch->name, udev->product, sizeof(usbtouch->name));
	}

	if (!strlen(usbtouch->name))
		snprintf(usbtouch->name, sizeof(usbtouch->name),
			"USB Touchscreen %04x:%04x",
			 le16_to_cpu(udev->descriptor.idVendor),
			 le16_to_cpu(udev->descriptor.idProduct));

	usb_make_path(udev, usbtouch->phys, sizeof(usbtouch->phys));
	strlcat(usbtouch->phys, "/input0", sizeof(usbtouch->phys));

	
	input_dev->name = usbtouch->name;
	input_dev->phys = usbtouch->phys;
	usb_to_input_id(udev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	input_set_drvdata(input_dev, usbtouch);

	input_dev->open = usbtouch_open;
	input_dev->close = usbtouch_close;

	if( !multi_point )
        {
#ifdef	DEBUG
           printk("---------------Single-point--------------\n");
#endif
		   input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
           input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
           input_set_abs_params(input_dev, ABS_X, type->min_xc, type->max_xc, 0, 0);
           input_set_abs_params(input_dev, ABS_Y, type->min_yc, type->max_yc, 0, 0);
           if (type->max_press)
                   input_set_abs_params(input_dev, ABS_PRESSURE, type->min_press,
                                        type->max_press, 0, 0);
       }
	else
       {
        __set_bit(EV_SYN, input_dev->evbit);
        __set_bit(EV_KEY, input_dev->evbit);
        __set_bit(EV_ABS, input_dev->evbit);
        __set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
        __set_bit(ABS_MT_POSITION_X, input_dev->absbit);
        __set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
        __set_bit(BTN_TOUCH, input_dev->keybit);

        input_set_abs_params(input_dev, ABS_X, 0, type->max_xc, 0, 0);
        input_set_abs_params(input_dev, ABS_Y, 0, type->max_yc, 0, 0);
        input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0, 0);
        input_set_abs_params(input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);
        input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, type->max_xc, 0, 0);
        input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, type->max_yc, 0, 0);
        input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
        input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
      }

	if (usb_endpoint_type(endpoint) == USB_ENDPOINT_XFER_INT)
		usb_fill_int_urb(usbtouch->irq, udev,
			 usb_rcvintpipe(udev, endpoint->bEndpointAddress),
			 usbtouch->data, type->rept_size,
			 usbtouch_irq, usbtouch, endpoint->bInterval);
	else
		usb_fill_bulk_urb(usbtouch->irq, udev,
			 usb_rcvbulkpipe(udev, endpoint->bEndpointAddress),
			 usbtouch->data, type->rept_size,
			 usbtouch_irq, usbtouch);
	
	usbtouch->irq->dev = udev;
	usbtouch->irq->transfer_dma = usbtouch->data_dma;
	usbtouch->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* device specific init */
	if (type->init) {
		err = type->init(usbtouch);
		if (err) {
			dbg("%s - type->init() failed, err: %d", __func__, err);
			goto out_free_urb;
		}
	}

	
	err = input_register_device(usbtouch->input);
		if (err) {
			dbg("%s - input_register_device failed, err: %d", __func__, err);
			goto out_do_exit;
		}

	usb_set_intfdata(intf, usbtouch);

	if (usbtouch->type->irq_always) {
		err = usb_submit_urb(usbtouch->irq, GFP_KERNEL);
		if (err) {
			err("%s - usb_submit_urb failed with result: %d",
			    __func__, err);
			goto out_unregister_input;
		}
	}
	
	return 0;

out_unregister_input:
	input_unregister_device(input_dev);
	input_dev = NULL;
out_do_exit:
	if (type->exit)
		type->exit(usbtouch);
out_free_urb:
	usb_free_urb(usbtouch->irq);
out_free_buffers:
	usbtouch_free_buffers(udev, usbtouch);
out_free:
	input_free_device(input_dev);
	kfree(usbtouch);
#ifdef	DEBUG
       printk("--------------------------------------------usbtouch_probe----------------------------------5\n");
#endif
	return err;
		
}

static void usbtouch_disconnect(struct usb_interface *intf)
{
#ifdef	DEBUG
    printk("--------------------------------------------usbtouch_disconnect----------------------------------\n");
#endif

	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);

	dbg("%s - called", __func__);

	if (!usbtouch)
		return;

	dbg("%s - usbtouch is initialized, cleaning up", __func__);
	usb_set_intfdata(intf, NULL);
	/* this will stop IO via close */
	input_unregister_device(usbtouch->input);
	usb_free_urb(usbtouch->irq);
	if (usbtouch->type->exit)
		usbtouch->type->exit(usbtouch);
	usbtouch_free_buffers(interface_to_usbdev(intf), usbtouch);
	kfree(usbtouch);
}


MODULE_DEVICE_TABLE(usb, usbtouch_devices);

static struct usb_driver usbtouch_driver = {
	.name		= "hctusbtouchscreen",
	.probe		= usbtouch_probe,
	.disconnect	= usbtouch_disconnect,
	.id_table	= usbtouch_devices,
};

static int __init hctusbtouch_init(void)
{
	return usb_register(&usbtouch_driver);
}

static void __exit hctusbtouch_cleanup(void)
{
	usb_deregister(&usbtouch_driver);
}

module_init(hctusbtouch_init);
module_exit(hctusbtouch_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

MODULE_ALIAS("touchkitusb");
MODULE_ALIAS("itmtouch");
MODULE_ALIAS("mtouchusb");


