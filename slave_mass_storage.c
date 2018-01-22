/* The Mass Storage Gadget acts as a USB Mass Storage device,
 * appearing to the host as a disk drive or as a CD-ROM drive.  In
 * addition to providing an example of a genuinely useful gadget
 * driver for a USB device,
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>

#include "f_mass_storage.h"

/*-------------------------------------------------------------------------*/

#define DRIVER_DESC 		"Mass Storage Gadget"
#define DRIVER_VERSION          "0.1"

#define FSG_VENDOR_ID   0x0525  /* NetChip */
#define FSG_PRODUCT_ID  0xa4a5  /* Linux-USB File-backed Storage Gadget */

/*-------------------------------------------------------------------------*/
USB_GADGET_COMPOSITE_OPTIONS();
/*-------------------------------------------------------------------------*/
/*USB_DT_DEVICE: Device descriptor*/
static struct usb_device_descriptor msg_device_desc = {
	.bLength =              sizeof msg_device_desc,
        .bDescriptorType =      USB_DT_DEVICE,

        /* .bcdUSB = DYNAMIC */
        .bDeviceClass =         USB_CLASS_PER_INTERFACE,  /* for DeviceClass */

        /* Vendor and product id can be overridden by module parameters.  */
        .idVendor =             cpu_to_le16(FSG_VENDOR_ID),
        .idProduct =            cpu_to_le16(FSG_PRODUCT_ID),
        .bNumConfigurations =   1,
};
/* All standard descriptors have these 2 fields at the beginning */
static const struct usb_descriptor_header *otg_desc[2];

/*-------------------------------------------------------------------------*/

/* utility to simplify dealing with string descriptors */

/**
 * struct usb_string - wraps a C string and its USB id
 * @id:the (nonzero) ID for this string
 * @s:the string, in UTF-8 encoding
 *
 * If you're using usb_gadget_get_string(), use this to wrap a string
 * together with its ID.
 */
static struct usb_string strings_dev[] = {
        [USB_GADGET_MANUFACTURER_IDX].s = "",
        [USB_GADGET_PRODUCT_IDX].s = DRIVER_DESC,
        [USB_GADGET_SERIAL_IDX].s = "",
        {  } /* end of list */
};

/**
 * struct usb_gadget_strings - a set of USB strings in a given language
 * @language:identifies the strings' language (0x0409 for en-us)
 * @strings:array of strings with their ids
 *
 * If you're using usb_gadget_get_string(), use this to wrap all the
 * strings for a given language.
 */
static struct usb_gadget_strings stringtab_dev = {
        .language       = 0x0409,       /* en-us */
        .strings        = strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
        &stringtab_dev,
        NULL,                   
};  

static struct usb_function_instance *fi_msg;
/* struct usb_function - describes one function of a configuration */
static struct usb_function *f_msg;


/****************************** Configurations ******************************/

static struct fsg_module_parameters mod_data = {
        .stall = 1
};

#ifdef CONFIG_USB_GADGET_DEBUG_FILES
           
static unsigned int fsg_num_buffers = CONFIG_USB_GADGET_STORAGE_NUM_BUFFERS;
           
#else      
           
/*
 * Number of buffers we will use.
 * 2 is usually enough for good buffering pipeline
 */     
#define fsg_num_buffers CONFIG_USB_GADGET_STORAGE_NUM_BUFFERS
        
#endif /* CONFIG_USB_GADGET_DEBUG_FILES */

FSG_MODULE_PARAMETERS(/* no prefix */, mod_data);

/**
 * gadget_is_otg - return true iff the hardware is OTG-ready
 * @g: controller that might have a Mini-AB connector
 *
 * This is a runtime test, since kernels with a USB-OTG stack sometimes
 * run on boards which only have a Mini-B (or Mini-A) connector.
 */
/**
 * usb_add_function() - add a function to a configuration
 * @config: the configuration
 * @function: the function being added
 * Context: single threaded during gadget setup
 *
 * After initialization, each configuration must have one or more
 * functions added to it.  Adding a function involves calling its @bind()
 * method to allocate resources such as interface and string identifiers
 * and endpoints.
 *
 * This function returns the value of the function's bind(), which is
 * zero for success else a negative errno value.
 */
static int msg_do_config(struct usb_configuration *c)
{
	struct fsg_opts *opts;
	int ret;
	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
                c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}
	opts = fsg_opts_from_func_inst(fi_msg);
	f_msg = usb_get_function(fi_msg);
	if (IS_ERR(f_msg))
                return PTR_ERR(f_msg);
	ret = usb_add_function(c, f_msg);
	if (ret)
                goto put_func;
        return 0;
put_func:
        usb_put_function(f_msg);
        return ret;
}
/**
 * struct usb_configuration - represents one gadget configuration
 * @label: For diagnostics, describes the configuration.
 * @bConfigurationValue: Copied into configuration descriptor.
 * @iConfiguration: Copied into configuration descriptor.
 * @bmAttributes: Copied into configuration descriptor.
 * @cdev: assigned by @usb_add_config() before calling @bind(); this is
 *	the device associated with this configuration.
 * Configurations are building blocks for gadget drivers structured around
 * function drivers.  Simple USB gadgets require only one function and one
 * configuration, and handle dual-speed hardware by always providing the same
 * functionality.  Slightly more complex gadgets may have more than one
 * single-function configuration at a given speed; or have configurations
 * that only work at one speed.
 */
static struct usb_configuration msg_config_driver = {
	.label                  = "Linux File-Backed Storage",
	.bConfigurationValue    = 1,
        .bmAttributes           = USB_CONFIG_ATT_SELFPOWER, /* self powered */
};

/****************************** Gadget Bind ******************************/
/**
 * usb_string_ids() - allocate unused string IDs in batch
 * @cdev: the device whose string descriptor IDs are being allocated
 * @str: an array of usb_string objects to assign numbers to
 * Context: single threaded during gadget setup
 *
 * @usb_string_ids() is called from bind() callbacks to allocate
 * string IDs.  Drivers for functions, configurations, or gadgets will
 * then copy IDs from the string table to the appropriate descriptors
 * and string table for other languages.
 *
 * All string identifier should be allocated using this,
 * @usb_string_id() or @usb_string_ids_n() routine, to ensure that for
 * example different functions don't wrongly assign different meanings
 * to the same identifier.
 */
/**
 * usb_add_config() - add a configuration to a device.
 * @cdev: wraps the USB gadget
 * @config: the configuration, with bConfigurationValue assigned
 * @bind: the configuration's bind function
 * Context: single threaded during gadget setup
 *
 * One of the main tasks of a composite @bind() routine is to
 * add each of the configurations it supports, using this routine.
 *
 * This function returns the value of the configuration's @bind(), which
 * is zero for success else a negative errno value.  Binding configurations
 * assigns global resources including string IDs, and per-configuration
 * resources such as interface IDs and endpoints.
 */
static int msg_bind(struct usb_composite_dev *cdev)
{
	struct fsg_opts *opts;
        struct fsg_config config;
        int status;
	fi_msg = usb_get_function_instance("mass_storage");
	if (IS_ERR(fi_msg))
                return PTR_ERR(fi_msg);
	fsg_config_from_params(&config, &mod_data, fsg_num_buffers);
	opts = fsg_opts_from_func_inst(fi_msg);
	opts->no_configfs = true;
	status = fsg_common_set_num_buffers(opts->common, fsg_num_buffers);
	if (status)
                goto fail;
	status = fsg_common_set_cdev(opts->common, cdev, config.can_stall);
	if (status)
                goto fail_set_cdev;
	fsg_common_set_sysfs(opts->common, true);
        status = fsg_common_create_luns(opts->common, &config);
        if (status)
                goto fail_set_cdev;
	fsg_common_set_inquiry_string(opts->common, config.vendor_name,
                                      config.product_name);
        status = usb_string_ids_tab(cdev, strings_dev);
        if (status < 0)
                goto fail_string_ids;
	msg_device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;
	
	if (gadget_is_otg(cdev->gadget) && !otg_desc[0]) {
		struct usb_descriptor_header *usb_desc;

		usb_desc = usb_otg_descriptor_alloc(cdev->gadget);
		if (!usb_desc)
			goto fail_string_ids;
		usb_otg_descriptor_init(cdev->gadget, usb_desc);
		otg_desc[0] = usb_desc;
		otg_desc[1] = NULL;
	}
	status = usb_add_config(cdev, &msg_config_driver, msg_do_config);
	if (status < 0)
		goto fail_otg_desc;

	usb_composite_overwrite_options(cdev, &coverwrite);
	dev_info(&cdev->gadget->dev,
		 DRIVER_DESC ", version: " DRIVER_VERSION "\n");
	return 0;
fail_otg_desc:
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
fail_string_ids:
	fsg_common_remove_luns(opts->common);
fail_set_cdev:
	fsg_common_free_buffers(opts->common);
fail:
	usb_put_function_instance(fi_msg);
	return status;
}
	
static int msg_unbind(struct usb_composite_dev *cdev)
{
	if (!IS_ERR(f_msg))
		usb_put_function(f_msg);

	if (!IS_ERR(fi_msg))
		usb_put_function_instance(fi_msg);

	kfree(otg_desc[0]);
	otg_desc[0] = NULL;

	return 0;
}
	

/*******************************************************************/
/**
 * struct usb_composite_driver - groups configurations into a gadget
 * @name: For diagnostics, identifies the driver.
 * @dev: Template descriptor for the device, including default device
 *      identifiers.
 * @strings: tables of strings, keyed by identifiers assigned during @bind
 *      and language IDs provided in control requests. Note: The first entries
 *      are predefined. The first entry that may be used is
 *      USB_GADGET_FIRST_AVAIL_IDX
 * @max_speed: Highest speed the driver supports.
 * @needs_serial: set to 1 if the gadget needs userspace to provide
 *      a serial number.  If one is not provided, warning will be printed.
 * @bind: (REQUIRED) Used to allocate resources that are shared across the
 *      whole device, such as string IDs, and add its configurations using
 *      @usb_add_config(). This may fail by returning a negative errno
 *      value; it should return zero on successful initialization.
 * @unbind: Reverses @bind; called as a side effect of unregistering
 *      this driver.
 */

static struct usb_composite_driver msg_driver = {
	.name 		= "g_mass_storage",
	.dev            = &msg_device_desc,  /*Descriptor for the device*/
	.max_speed      = USB_SPEED_SUPER,
	.needs_serial   = 1,
        .strings        = dev_strings,
        .bind           = msg_bind,
        .unbind         = msg_unbind,

};

/**
 * usb_composite_probe() - register a composite driver
 * @driver: the driver to register
 *
 * Context: single threaded during gadget setup
 *
 * This function is used to register drivers using the composite driver
 * framework.  The return value is zero, or a negative errno value.
 * Those values normally come from the driver's @bind method, which does
 * all the work of setting up the driver to match the hardware.
 *
 * On successful return, the gadget is ready to respond to requests from
 * the host, unless one of its components invokes usb_gadget_disconnect()
 * while it was binding.  That would usually be done in order to wait for
 * some userspace participation.
 */
static int __init msg_init(void)
{
	return usb_composite_probe(&msg_driver);
}

/**
 * usb_composite_unregister() - unregister a composite driver
 * @driver: the driver to unregister
 *
 * This function is used to unregister drivers using the composite
 * driver framework.
 */
static void __exit msg_cleanup(void)
{
	usb_composite_unregister(&msg_driver);
}

module_init(msg_init);
module_exit(msg_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Chandan Jha <beingchandanjha@gmail.com>");
MODULE_LICENSE("GPL");
