#ifndef _K_GODCDEV_H_
#define _K_GODCDEV_H_

typedef unsigned int opcode_t;
typedef unsigned int procflag_t;

/* general return codes */
#define EXIT_SUCCESS                0
#define EXIT_FAILURE                1

/* number base(s) */
#define BASE_DECIMAL                10
#define BASE_HEXADECIMAL            16


/* God-Mode (SIGNAL_UNKILLABLE) */
#define SIGNAL_UNKILLABLE   		0x00000040              /* SIGNAL_UNKILLABLE */
#define FLAG_DEFAULT	    		0x00000000              /* flag reset to force-clear SIGNAL_UNKILLABLE */


#define TASK_INVALID				0xFFFFFFFF				/* If task_struct == NULL, return UINT_MAX, so we won't confuse the erroneous operation with actual signal flag */ 


/* --- Character Device --- */

/* character device status (semaphore) identifiers */
#define DEV_STATUS_ERROR            2       				/* status == error            */
#define DEV_STATUS_INITIALIZED      1       				/* status == initialized (OK) */
#define DEV_STATUS_UNINITIALIZED    0       				/* status == not initialized  */

/* character device operations */
#define CDEV_UNREGISTER_ONLY        2              			/* used for unregistering chrdev only */
#define CDEV_DESTROY_CLASS_ONLY     1              			/* used to specify if unregisted_device() should only destroy the class itself  */
#define CDEV_DESTROY_DEVICE_ONLY    0              			/* used to specify if unregister_device() should only destroy the chrdev itself */

/* character device constants */
#define DATA_MAX                    96              		/* max size of data copied from user */
#define CDEV_NAME                   "gmdDevuce"     		/* cdev name */
#define CDEV_CLASS_NAME             "gmdev"         		/* cdev class name */

/* init() exit codes */
#define ECDEV                       256            			/* unable to spawn chardev */
#define ESCNR                       257            			/* unable to deploy heavy-duty scanners */

/* godMode cdev com opcodes */
#define GMDEV_ENABLE_GOD			1
#define GMDEV_DISABLE_GOD			0


#endif
