#include <linux/version.h>                              /* version */

/* Check if our kernel version is newer than 5.9.0 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
#error "GodMode Device requires Linux Kernel 5.9.0 or newer"
#endif

#include "k_gmdev.h"

#include <asm/types.h>
#include <linux/pid.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>                                                    /* msleep() */
#include <linux/device.h>                                                   /* cdev mgmt */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yehkub");
MODULE_INFO(intree, "Y");
MODULE_DESCRIPTION("godMode Device");


/* --- runtime variables --- */
static int major;                                                           /* character device major number */
static int deviceMutex = 0;                                                 /* cdev semaphore */

/* --- cdev IO --- */
static uint8_t chrdev_reg_status        = DEV_STATUS_UNINITIALIZED;         /* status semaphore for device registration */
static uint8_t chrdev_class_status      = DEV_STATUS_UNINITIALIZED;         /* status semaphore for cdev_class  */
static uint8_t chrdev_device_status     = DEV_STATUS_UNINITIALIZED;         /* status semaphore for cdev_device */

static struct class* gmdClass   = NULL;                                     /* cdev class */
static struct device* gmdDevice = NULL;                                     /* cdev device struct */


/* --- CDEV --- */
static int exec_operation(pid_t *procid);

/* _open cdev */
static int _open(struct inode *i, struct file *f) {
    /* static int counter = 0; */
    if (deviceMutex) {
        printk(KERN_WARNING "[gmDev]: Busy RW attempt\n");
        return -EBUSY;
    }

    deviceMutex++;                                                          /* semaphore red light */
    try_module_get(THIS_MODULE);
    return EXIT_SUCCESS;
}

/* _close cdev */
static int _close(struct inode *i, struct file *f) {
    deviceMutex--;                                                          /* semaphore green light */
    module_put(THIS_MODULE);

    return EXIT_SUCCESS;
}

/* _read 'user -> kernel', ssize_t to also return errors, we don't need that for right now.. */
static ssize_t _read(struct file *f, char *buf, size_t len, loff_t *off) {
    return EXIT_SUCCESS;
}

/* _write `copy_from_user()`, this is the communication date with our cdev from user space */
static ssize_t _write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    size_t bytes_copied, ncopied;                                           /* bytes copied & bytes_not_copied */
    uint8_t databuf[DATA_MAX];                                              /* data buffer */
    long oppid;                                                             /* pid of process to perform the surgery on */
    int ctl_ret;                                                            /* kstrtol return value */

    if (count > DATA_MAX) {
        return -EXIT_FAILURE;                                               /* overflow if count > DATA_MAX */
    }
    
    ncopied = copy_from_user(databuf, buf, DATA_MAX);                       /* copy_from_user() into `databuf`, `copy_from_user()` returns number of bytes that could NOT be copied, so we save this to substract the value from DATA_MAX later */
    if (ncopied != 0) {
        return -EXIT_FAILURE;
    }

    bytes_copied = count - ncopied;                                         /* substract n(ot)copied from total number of bytes */

    databuf[bytes_copied] = 0;                                              /* explicitly terminate the last byte */


    /* convert databuf to long */
    ctl_ret = kstrtol(databuf, BASE_DECIMAL, &oppid);

    if (ctl_ret != EXIT_SUCCESS) {
        printk(KERN_INFO "[gmDev]: Ignoring invalid PID\n");
        return -ctl_ret;
    }

    exec_operation( (pid_t *) &oppid);
    return bytes_copied;
}

/* file operations for character device */
static struct file_operations fops = 
{
    .owner      = THIS_MODULE,
    .open       = _open,
    .release    = _close,
    .read       = _read,
    .write      = _write
};

/* destroy character device */
static int unregister_device(void) {
    if (chrdev_device_status != DEV_STATUS_UNINITIALIZED)                   /* check semaphores first */
        device_destroy(gmdClass, MKDEV(major, 0));

    if (chrdev_class_status != DEV_STATUS_UNINITIALIZED)
        class_unregister(gmdClass);    
    
    if (chrdev_reg_status != DEV_STATUS_UNINITIALIZED)
        unregister_chrdev(major, CDEV_NAME);

    printk(KERN_INFO "[gmDev]: Unregistering godMode Device..\n");
    return EXIT_SUCCESS;
}

/* spawn a character device */
static int create_cdev(void) {
    major = register_chrdev(0, CDEV_NAME, &fops);                           /* register device and ask for dynamic major number allocation */

    /* check if dynamic major number allocation failed */
    if (major < 0) {
        printk(KERN_ERR "[gmDev]: Couldn't assign device major number - %d\n", major);
        /* leave chrdev_reg_status set as UNITIALIZED, so unregister_device() doesn't attempt to destroy it when it doesn't exist.. */
        return -EXIT_FAILURE;
    }

    chrdev_reg_status = DEV_STATUS_INITIALIZED;                             /* device was registered successfully, mark it down */

    gmdClass = class_create(THIS_MODULE, CDEV_CLASS_NAME);

    if (IS_ERR(gmdClass) || gmdClass == NULL) {                             /* IS_ERR() macro doesn't see NULL as an error */
        printk(KERN_ERR "[gmDev]: Device class registration failed with error code %ld\n", PTR_ERR(gmdClass));
        chrdev_class_status = DEV_STATUS_ERROR;
        unregister_device();
        return PTR_ERR(gmdClass);
    }

    chrdev_class_status = DEV_STATUS_INITIALIZED;

    gmdDevice = device_create(gmdClass, NULL, MKDEV(major, 0), NULL, CDEV_CLASS_NAME);

    if (IS_ERR(gmdDevice) || gmdDevice == NULL) {
        printk(KERN_ERR "[gmDev]: Device identifier registration failed with error code %ld\n", PTR_ERR(gmdDevice));
        chrdev_device_status = DEV_STATUS_ERROR;
        unregister_device();
        return PTR_ERR(gmdDevice);
    }

    chrdev_device_status = DEV_STATUS_INITIALIZED;

    printk(KERN_INFO "[gmDev]: Successfully registered godMode character device!\n");
    return EXIT_SUCCESS;

}

/* Read process signal flags */
procflag_t read_proc_sigflag(pid_t *procid) {
    char *str_flagptr;
    char str_flagdefault[] = "FLAG_DEFAULT";
    char str_sigunkillable[] = "SIGNAL_UNKILLABLE";

    struct task_struct *task = NULL;

    task = pid_task(find_get_pid(*procid), PIDTYPE_PID);

    if (task) {
        str_flagptr = task->signal->flags ? str_sigunkillable : str_flagdefault;
        printk(KERN_INFO "[gmDev]: Task flags for PID: %d - %s (0x%x | %u)\n", *procid, str_flagptr, task->signal->flags, task->signal->flags);
        return task->signal->flags;
    }

    return TASK_INVALID;
}

/* Assign GodMode */
int op_assign_gmode(pid_t *pid) {
    struct task_struct *task = NULL;
    
    if (*pid < 1) { return EXIT_FAILURE; }

    task = pid_task(find_get_pid(*pid), PIDTYPE_PID);

    if (task) {
        printk(KERN_INFO "[gmDev]: Assigned flag SIGNAL_UNKILLABLE to PID(%d)\n", *pid);
        task->signal->flags |= SIGNAL_UNKILLABLE;                             /* assign godmode */
        return EXIT_SUCCESS;
    }

    printk(KERN_INFO "[gmDev]: Unable to assign SIGNAL_UNKILLABLE to PID(%d)\n", *pid);

    return EXIT_FAILURE;
}

/* Revoke GodMode */
int op_revoke_gmode(pid_t *pid) {
    struct task_struct *task = NULL;
    
    if (*pid < 1) { return EXIT_FAILURE; }

    task = pid_task(find_get_pid(*pid), PIDTYPE_PID);

    if (task) {
        printk(KERN_INFO "[gmDev]: Assigned flag FLAG_DEFAULT to PID(%d)\n", *pid);
        task->signal->flags = FLAG_DEFAULT;       
        return EXIT_SUCCESS;
    }

    printk(KERN_INFO "[gmDev]: Unable to assign FLAG_DEFAULT to PID(%d)\n", *pid);
    return EXIT_FAILURE;

}

static int exec_operation(pid_t *procid) {
    procflag_t cpflags;
    unsigned int rtcode = EXIT_SUCCESS;

    if (procid == NULL) { return EXIT_FAILURE; }
    
    cpflags = read_proc_sigflag(procid);

    switch (cpflags) {
        case FLAG_DEFAULT:
            rtcode = op_assign_gmode(procid);
            break;
        
        case SIGNAL_UNKILLABLE:
            rtcode = op_revoke_gmode(procid);
            break;

        case TASK_INVALID:
            printk(KERN_INFO "[gmDev]: No such process: %d", *procid);
            rtcode = EXIT_FAILURE;
            break;
    
        default:
            rtcode = EXIT_FAILURE;
            break;
    }

    return rtcode;
}

/* --- Entry Point --- */
int init_module(void) {
    
    int cdevret;
    
    printk(KERN_INFO "[gmDev]: Starting godMode Device..\n");

    cdevret = create_cdev();
    if (cdevret != EXIT_SUCCESS) {
        printk(KERN_ERR "[gmDev]: E%d - Failed to initialize gmDev\n", ECDEV);
        return ECDEV;
    }


    return EXIT_SUCCESS;
}

/* --- Exit Point --- */
void cleanup_module(void) {
    unregister_device();

    printk(KERN_INFO "[gmDev]: Unloading godMode Device from kernel\n");
}
