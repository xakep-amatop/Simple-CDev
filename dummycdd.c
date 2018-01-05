/**
 * @file   dummycdd.c
 * @author Mykola Kvach
 * @date   26 October 2017
 * @version 0.1
 * @brief  Linux character device driver. This module maps to /dev/dummycdd[id=1]
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function

#define  DEVICE_NAME "dummycdd"     ///< The device will appear at /dev/dummycdd[id] using this value
#define  CLASS_NAME  "dummycdd"     ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");              ///< The license type -- this affects available functionality
MODULE_AUTHOR("Mykola Kvach");      ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver");  ///< The description -- see modinfo
MODULE_VERSION("0.1");              ///< A version number to inform users

static int    majorNumber;                ///< Stores the device number -- determined automatically
static char   message[256]        = {0};  ///< Memory for the string that is passed from userspace
static int    numberOpens         = 0;    ///< Counts the number of times the device is opened
static struct class*  dummyClass  = NULL; ///< The device-driver class struct pointer
static struct device* dummyDevice = NULL; ///< The device-driver device struct pointer

static char   device_name[32]       = {0};
static int    id                    = 1;  ///< The minor device number

module_param(id, int, S_IRUGO);

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open    (struct inode *, struct file *);
static int     dev_release (struct inode *, struct file *);
static ssize_t dev_write   (struct file *,  const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
  .open = dev_open,
  .write = dev_write,
  .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init dummycdd_init(void){
  sprintf(device_name, DEVICE_NAME"%d", id);

  printk(KERN_INFO "%s: Initializing the %s LKM\n", device_name, device_name);

  // Try to dynamically allocate a major number for the device
  majorNumber = register_chrdev(0, device_name, &fops);
  if (majorNumber<0){
    printk(KERN_ALERT "%s failed to register a major number\n", device_name);
    return majorNumber;
  }
  printk(KERN_INFO "%s: registered correctly with major number %d\n", device_name, majorNumber);

  // Register the device class
  dummyClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(dummyClass)){                // Check for error and clean up if there is
    unregister_chrdev(majorNumber, device_name);
    printk(KERN_ALERT "%s: Failed to register device class\n", device_name);
    return PTR_ERR(dummyClass);          // Correct way to return an error on a pointer
  }
  printk(KERN_INFO "%s: device class registered correctly\n", device_name);

  // Register the device driver
  dummyDevice = device_create(dummyClass, NULL, MKDEV(majorNumber, 0), NULL, device_name);
  if (IS_ERR(dummyDevice)){              // Clean up if there is an error
    class_destroy(dummyClass);           // Repeated code but the alternative is goto statements
    unregister_chrdev(majorNumber, device_name);
    printk(KERN_ALERT "%s: Failed to create the device\n", device_name);

    return PTR_ERR(dummyDevice);
  }
  printk(KERN_INFO "%s: device class created correctly\n", device_name); // Made it! device was initialized
  return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit dummycdd_exit(void){
  device_destroy(dummyClass, MKDEV(majorNumber, 0));    // remove the device
  class_unregister(dummyClass);                          // unregister the device class
  class_destroy(dummyClass);                             // remove the device class
  unregister_chrdev(majorNumber, device_name);             // unregister the major number
  printk(KERN_INFO "%s: Goodbye from the LKM!\n", device_name);
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep  A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
  numberOpens++;
  printk(KERN_INFO "%s: Device has been opened %d time(s)\n", device_name, numberOpens);
  return 0;
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the copy_from_user() function.
 *  @param filep    A pointer to a file object
 *  @param buffer   The buffer to that contains the string to write to the device
 *  @param len The  length of the array of data that is being passed in the const char buffer
 *  @param offset   The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
  long long i = 0;
  printk(KERN_INFO "%s: Received %zu characters from the user. Message: ", device_name, len);

  for(; i < len; i+=sizeof(message)){
    size_t remainder = len - i;
    if(remainder > sizeof(message)){
      copy_from_user(message, buffer + i, sizeof(message));
    }
    else{
      copy_from_user(message, buffer + i, remainder);
    }
    printk(KERN_INFO "%s", message);
  }
  return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep  A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "%s: Device successfully closed\n", device_name);
  return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(dummycdd_init);
module_exit(dummycdd_exit);