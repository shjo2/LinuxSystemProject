#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#define BUF_SIZE	1024
#define WAIT_QUEUE_WAIT 0
#define WAIT_QUEUE_KEY 1
#define WAIT_QUEUE_NEXT 2
#define WAIT_QUEUE_EXIT 3
/* left leds */
#define TOY_GPIO_OUTPUT_LEFT_1 17
#define TOY_GPIO_OUTPUT_LEFT_2 27
#define TOY_GPIO_OUTPUT_LEFT_3 22
#define TOY_GPIO_OUTPUT_LEFT_4 5
/* right leds */
#define TOY_GPIO_OUTPUT_RIGHT_1 6
#define TOY_GPIO_OUTPUT_RIGHT_2 26
#define TOY_GPIO_OUTPUT_RIGHT_3 23
#define TOY_GPIO_OUTPUT_RIGHT_4 24
#define TOY_GPIO_INPUT 16

static char kernel_write_buffer[BUF_SIZE];
static dev_t toy_dev;
static struct class *toy_class;
static struct cdev toy_device;
static struct task_struct *wait_thread;
int wait_queue_flag = WAIT_QUEUE_WAIT;
unsigned int button_irq;

static int register_gpio_output(int gpio_num);

DECLARE_WAIT_QUEUE_HEAD(wait_queue);

#define DRIVER_NAME "toy_simple_io_driver"
#define DRIVER_CLASS "toy_simple_io_class"
#define ON  1
#define OFF 0

static const unsigned char led_array_map[8] = {
	TOY_GPIO_OUTPUT_LEFT_1,
	TOY_GPIO_OUTPUT_LEFT_2,
	TOY_GPIO_OUTPUT_LEFT_3,
	TOY_GPIO_OUTPUT_LEFT_4,
	TOY_GPIO_OUTPUT_RIGHT_1,
	TOY_GPIO_OUTPUT_RIGHT_2,
	TOY_GPIO_OUTPUT_RIGHT_3,
	TOY_GPIO_OUTPUT_RIGHT_4
};

#define LED_LEFT_ON() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_1, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_2, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_3, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_4, 1);	\
} while (0)

#define LED_LEFT_OFF() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_1, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_2, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_3, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_4, 0);	\
} while (0)

#define LED_RIGHT_ON() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_1, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_2, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_3, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_4, 1);	\
} while (0)

#define LED_RIGHT_OFF() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_1, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_2, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_3, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_4, 0);	\
} while (0)

static ssize_t toy_driver_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{

	return BUF_SIZE;
}

ssize_t toy_driver_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	if (copy_from_user(kernel_write_buffer, buf, count)) {
		pr_err("write: error\n");
	}

	switch (kernel_write_buffer[0]) {
		case 0:
			wait_queue_flag = WAIT_QUEUE_WAIT;
			pr_info("set WAIT_QUEUE_WAIT!\n");
			break;
		case 1:
			wait_queue_flag = WAIT_QUEUE_KEY;
			wake_up_interruptible(&wait_queue);
			pr_info("set WAIT_QUEUE_KEY!\n");
			break;
		case 2:
			wait_queue_flag = WAIT_QUEUE_NEXT;
			wake_up_interruptible(&wait_queue);
			pr_info("set WAIT_QUEUE_EXIT!\n");
			break;
		default:
			pr_info("Invalid Input!\n");
			break;
	}

	pr_info("write: done\n");

	return count;
}

static int toy_driver_open(struct inode *device_file, struct file *instance)
{
	pr_info("open\n");
	return 0;
}

static int toy_driver_close(struct inode *device_file, struct file *instance)
{
	pr_info("close\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = toy_driver_open,
	.release = toy_driver_close,
	.read = toy_driver_read,
	.write = toy_driver_write
};

void set_bit_led(unsigned char led)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (led & (1 << i))
			gpio_set_value(led_array_map[i], 1);
		else
			gpio_set_value(led_array_map[i], 0);
	}
}

int toy_state_machine_example_1(void)
{
	int state = 0;
	unsigned long hard_delay_timer_count;

	pr_info("\n 토이 state machine 실험 1\n");

	set_bit_led(0x00);
	wait_queue_flag = WAIT_QUEUE_WAIT;
loop:
	switch (state) {
		case 0 :
			if (gpio_get_value(TOY_GPIO_INPUT)) {
				hard_delay_timer_count = jiffies; /* Timer Start */
				state = 1;
			}
			break;
		case 1 :
			if (!gpio_get_value(TOY_GPIO_INPUT)) {
				set_bit_led(0x0F);
				state = 0;
			} else if (jiffies_to_msecs(jiffies - hard_delay_timer_count) >= 1000) {
				state = 2;
			}
			break;
		case 2 :
			if (!gpio_get_value(TOY_GPIO_INPUT)) {
				set_bit_led(0xF0);
				state = 0;
			}
			break;
		default :
			set_bit_led(0x00);
			state = 0;
			break;
	}
	usleep_range(1000, 1001);
	if (wait_queue_flag == WAIT_QUEUE_EXIT || wait_queue_flag == WAIT_QUEUE_NEXT) {
		return wait_queue_flag;
	}

	goto loop;
}

#define NUM_STATE	3
#define NUM_INPUT	3

enum { SW_ON, SW_OFF, T_OUT };
enum { ACTION_NOP, ACTION_LEFT_ON, ACTION_RIGHT_ON, ACTION_TS };

struct state_machine {
	int next_state;
	int action;
};

struct state_machine example_2_state_machine[NUM_STATE][NUM_INPUT] = {
	{	/* State 0 */
		{ 1, ACTION_TS  }, 	/* input SW_ON */
		{ 0, ACTION_NOP }, 	/* input SW_OFF */
		{ 0, ACTION_NOP }  	/* input T_OUT */
	},
	{	/* State 1 */
		{ 1, ACTION_NOP },		/* input SW_ON */
		{ 0, ACTION_LEFT_ON }, /* input SW_OFF */
		{ 2, ACTION_NOP }		/* input T_OUT */
	},
	{	/* State 2 */
		{ 2, ACTION_NOP },		/* input SW_ON */
		{ 0, ACTION_RIGHT_ON },/* input SW_OFF */
		{ 2, ACTION_NOP }		/* input T_OUT */
	}
};

int toy_state_machine_example_2(void)
{
	int state;
	int input;
	unsigned long hard_delay_timer_count;

	pr_info("\n 토이 state machine 실험 2\n");

	set_bit_led(0x00);
	wait_queue_flag = WAIT_QUEUE_WAIT;
	hard_delay_timer_count = jiffies; /* Timer Start */
	state = 0;
loop:
	/* 1: Generate Input (Event) */
	if ((state == 1) && (jiffies_to_msecs(jiffies - hard_delay_timer_count) >= 1000))
		input = T_OUT;
	else if (gpio_get_value(TOY_GPIO_INPUT))
		input = SW_ON;
	else
		input = SW_OFF;

	/* 2: Do action */
	switch (example_2_state_machine[state][input].action) {
		case ACTION_TS :
			hard_delay_timer_count = jiffies; /* Timer Start */
			break;
		case ACTION_LEFT_ON :
			pr_info("<Short>\n");
			set_bit_led(0x0F);
			break;
		case ACTION_RIGHT_ON :
			pr_info("<Long>\n");
			set_bit_led(0xF0);
			break;
		default :
			break;
	}

	/* 2: Set Next State */
	state = example_2_state_machine[state][input].next_state;
	usleep_range(1000, 1001);
	if (wait_queue_flag == WAIT_QUEUE_EXIT || wait_queue_flag == WAIT_QUEUE_NEXT) {
		return wait_queue_flag;
	}

	goto loop;
}

enum ex3_state {
    S_IDLE,             
    S_PRESSED,          
    S_SHORT_RELEASE,    
    S_LONG_PRESS,       
    S_LONG_RELEASE,     
    S_DCLICK_S_PRESS,   
    S_DCLICK_L_PRESS    
};

enum ex3_input {
    EV_PRESS,           
    EV_RELEASE,         
    EV_TIMEOUT          
};

enum ex3_action {
    AC_NOP,             
    AC_TIMER_START,     
    AC_LED_NEXT,        
    AC_LED_PREV,        
    AC_LED_L4_ON,       
    AC_LED_L6_ON,       
    AC_LED_ALL_ON,      
    AC_LED_ALL_OFF,     
};

#define NUM_STATE   7
#define NUM_INPUT   3

struct state_machine_ex3 {
    int next_state;
    int action;
};

struct state_machine_ex3 example_3_state_machine[NUM_STATE][NUM_INPUT] = {
    {   {S_PRESSED, AC_TIMER_START},    {S_IDLE, AC_NOP},           {S_IDLE, AC_NOP} },
    {   {S_PRESSED, AC_NOP},            {S_SHORT_RELEASE, AC_TIMER_START}, {S_LONG_PRESS, AC_NOP} },
    {   {S_DCLICK_S_PRESS, AC_TIMER_START}, {S_SHORT_RELEASE, AC_NOP},   {S_IDLE, AC_LED_NEXT} },
    {   {S_LONG_PRESS, AC_NOP},         {S_LONG_RELEASE, AC_TIMER_START}, {S_LONG_PRESS, AC_NOP} },
    {   {S_DCLICK_L_PRESS, AC_TIMER_START}, {S_LONG_RELEASE, AC_NOP},    {S_IDLE, AC_LED_L4_ON} },
    {   {S_DCLICK_S_PRESS, AC_NOP},     {S_IDLE, AC_LED_PREV},          {S_IDLE, AC_LED_L6_ON} },
    {   {S_DCLICK_L_PRESS, AC_NOP},     {S_IDLE, AC_LED_ALL_ON},        {S_IDLE, AC_LED_ALL_OFF} }
};


int toy_state_machine_example_3(void)
{
    int state = S_IDLE;
    int input;
    unsigned char led_state = 0x00;
    int last_gpio_state = 0;
    unsigned long timer_start_time = 0;

    set_bit_led(led_state);
    wait_queue_flag = WAIT_QUEUE_WAIT;
    last_gpio_state = gpio_get_value(TOY_GPIO_INPUT);

loop:
    int current_gpio_state = gpio_get_value(TOY_GPIO_INPUT);
    if (current_gpio_state && !last_gpio_state) {
        input = EV_PRESS;
    } else if (!current_gpio_state && last_gpio_state) {
        input = EV_RELEASE;
    } else {
        int timeout_occured = (jiffies_to_msecs(jiffies - timer_start_time) >= 1000);
        if ((state == S_PRESSED || state == S_SHORT_RELEASE || state == S_LONG_RELEASE ||
             state == S_DCLICK_S_PRESS || state == S_DCLICK_L_PRESS) && timeout_occured) {
            input = EV_TIMEOUT;
        } else {
            usleep_range(10000, 10001);
            goto loop;
        }
    }
    last_gpio_state = current_gpio_state;


    switch (example_3_state_machine[state][input].action) {
        case AC_TIMER_START:
            timer_start_time = jiffies;
            break;
        case AC_LED_NEXT: // Short Click
            pr_info("<Short Click>\n");
            if (led_state != 0xFF) led_state = (led_state << 1) | 1;
            set_bit_led(led_state);
            break;
        case AC_LED_PREV: // Short-Short Double Click
            pr_info("<Short-Short Double Click>\n");
            if (led_state != 0x00) led_state = led_state >> 1;
            set_bit_led(led_state);
            break;
        case AC_LED_L4_ON: // Long Click
            pr_info("<Long Click>\n");
            led_state = 0x0F;
            set_bit_led(led_state);
            break;
        case AC_LED_L6_ON: // Short-Long Double Click
            pr_info("<Short-Long Double Click>\n");
            led_state = 0x3F;
            set_bit_led(led_state);
            break;
        case AC_LED_ALL_ON: // Long-Short Double Click
            pr_info("<Long-Short Double Click>\n");
            led_state = 0xFF;
            set_bit_led(led_state);
            break;
        case AC_LED_ALL_OFF: // Long-Long Double Click
            pr_info("<Long-Long Double Click>\n");
            led_state = 0x00;
            set_bit_led(led_state);
            break;
        case AC_NOP:
        default:
            break;
    }

    state = example_3_state_machine[state][input].next_state;

    if (wait_queue_flag == WAIT_QUEUE_EXIT || wait_queue_flag == WAIT_QUEUE_NEXT) {
        return wait_queue_flag;
    }

    goto loop;
}

static int simple_io_kthread(void *unused)
{
	while (1) {
		if (toy_state_machine_example_1() == WAIT_QUEUE_EXIT)
			break;
		if (toy_state_machine_example_2() == WAIT_QUEUE_EXIT)
			break;
		if (toy_state_machine_example_3() == WAIT_QUEUE_EXIT)
			break;
	}
	//do_exit(0);
	return 0;
}

static int register_gpio_output(int gpio_num)
{
	char name[80];

	snprintf(name, sizeof(name), "toy-gpio-%d", gpio_num);

	if (gpio_request(gpio_num, name)) {
		pr_info("Can not allocate GPIO \n");
		return -1;
	}

	if(gpio_direction_output(gpio_num, 0)) {
		pr_info("Can not set GPIO  to output!\n");
		return -1;
	}

	return 0;
}

static int __init toy_module_init(void)
{
	if (alloc_chrdev_region(&toy_dev, 0, 1, DRIVER_NAME) < 0) {
		pr_info("Device Nr. could not be allocated!\n");
		return -1;
	}

	pr_info("할당 받은 Major = %d Minor = %d \n", MAJOR(toy_dev), MINOR(toy_dev));
	if ((toy_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		pr_info("Device class can not be created!\n");
		goto cerror;
	}

	if (device_create(toy_class, NULL, toy_dev, NULL, DRIVER_NAME) == NULL) {
		pr_info("Can not create device file!\n");
		goto device_error;
	}

	cdev_init(&toy_device, &fops);

	if (cdev_add(&toy_device, toy_dev, 1) == -1) {
		pr_info("Registering of device to kernel failed!\n");
		goto reg_error;
	}

	register_gpio_output(TOY_GPIO_OUTPUT_LEFT_1);
	register_gpio_output(TOY_GPIO_OUTPUT_LEFT_2);
	register_gpio_output(TOY_GPIO_OUTPUT_LEFT_3);
	register_gpio_output(TOY_GPIO_OUTPUT_LEFT_4);
	register_gpio_output(TOY_GPIO_OUTPUT_RIGHT_1);
	register_gpio_output(TOY_GPIO_OUTPUT_RIGHT_2);
	register_gpio_output(TOY_GPIO_OUTPUT_RIGHT_3);
	register_gpio_output(TOY_GPIO_OUTPUT_RIGHT_4);

	if(gpio_request(TOY_GPIO_INPUT, "toy-gpio-16")) {
		pr_info("Can not allocate GPIO 16\n");
		goto gpio_17_error;
	}

	if(gpio_direction_input(TOY_GPIO_INPUT)) {
		pr_info("Can not set GPIO 16 to input!\n");
		goto gpio_16_error;
	}

	wait_thread = kthread_create(simple_io_kthread, NULL, "simple io thread");
	if (wait_thread) {
		pr_info("Thread created successfully\n");
		wake_up_process(wait_thread);
	} else
		pr_info("Thread creation failed\n");


	return 0;

gpio_16_error:
	gpio_free(16);
gpio_17_error:
	gpio_free(17);
reg_error:
	device_destroy(toy_class, toy_dev);
device_error:
	class_destroy(toy_class);
cerror:
	unregister_chrdev_region(toy_dev, 1);
	return -1;
}

static void __exit toy_module_exit(void)
{
	wait_queue_flag = WAIT_QUEUE_EXIT;
	wake_up_interruptible(&wait_queue);
	gpio_set_value(16, 0);
	gpio_free(16);
	gpio_free(TOY_GPIO_OUTPUT_LEFT_1);
	gpio_free(TOY_GPIO_OUTPUT_LEFT_2);
	gpio_free(TOY_GPIO_OUTPUT_LEFT_3);
	gpio_free(TOY_GPIO_OUTPUT_LEFT_4);
	gpio_free(TOY_GPIO_OUTPUT_RIGHT_1);
	gpio_free(TOY_GPIO_OUTPUT_RIGHT_2);
	gpio_free(TOY_GPIO_OUTPUT_RIGHT_3);
	gpio_free(TOY_GPIO_OUTPUT_RIGHT_4);
	cdev_del(&toy_device);
	device_destroy(toy_class, toy_dev);
	class_destroy(toy_class);
	unregister_chrdev_region(toy_dev, 1);
	pr_info("exit\n");
}

module_init(toy_module_init);
module_exit(toy_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TOY <toy@toy.com>");
MODULE_DESCRIPTION("toy simple io");
MODULE_VERSION("1.0.0");
