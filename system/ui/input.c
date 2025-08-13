#include <assert.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <execinfo.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <seccomp.h>
 #include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <execinfo.h>
#include <toy_message.h>
#include <shared_memory.h>
#include <dump_state.h>

#define MOTOR_1_SET_SPEED _IOW('w', '1', int32_t *)
#define MOTOR_2_SET_SPEED _IOW('w', '2', int32_t *)

#define TOY_TOK_BUFSIZE 64
#define TOY_TOK_DELIM " \t\r\n\a"
#define TOY_BUFFSIZE 1024
#define DUMP_STATE 2

typedef struct _sig_ucontext {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask;
} sig_ucontext_t;

static pthread_mutex_t global_message_mutex  = PTHREAD_MUTEX_INITIALIZER;
static char global_message[TOY_BUFFSIZE];

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;
static mqd_t backend_queue;

static shm_sensor_t *the_sensor_info = NULL;

#define TOY_SYSFS_TRIGGER  "/sys/toy/trigger"
#define TOY_SYSFS_NOTIFY   "/sys/toy/notify"

/*
 *  sensor thread
 */
void *sensor_thread(void* arg)
{
    int mqretcode;
    char *s = arg;
    toy_msg_t msg;
    int notify_fd, rv;
    char buff[100];
    struct pollfd sensor_fd[1];
    int shmid = toy_shm_get_keyid(SHM_KEY_SENSOR);

    printf("%s", s);

    if ((notify_fd = open(TOY_SYSFS_NOTIFY, O_RDWR)) < 0){
        perror("Unable to open notify");
        exit(1);
    }

    sensor_fd[0].fd = notify_fd;
    sensor_fd[0].events = POLLPRI;

    while (1) {
        if (( rv = poll( sensor_fd, 1, 100000000)) < 0 )
            printf("poll error\n");
        else if (rv == 0)
            printf("Timeout occurred!\n");

        if (read( notify_fd, buff, 100 ) < 0)
            continue;

        printf("%s", buff);
        lseek(notify_fd, 0, SEEK_SET);

        if (the_sensor_info != NULL) {
            the_sensor_info->temp =  atoi(buff);
            the_sensor_info->press = 11;
            the_sensor_info->humidity = 80;
        }
        msg.msg_type = 1;
        msg.param1 = shmid;
        msg.param2 = 0;
        mqretcode = mq_send(monitor_queue, (char *)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);
    }

    close(notify_fd);

    return 0;
}

/*
 *  command thread
 */

int toy_send(char **args);
int toy_mutex(char **args);
int toy_shell(char **args);
int toy_message_queue(char **args);
int toy_read_elf_header(char **args);
int toy_dump_state(char **args);
int toy_mincore(char **args);
int toy_busy(char **args);
int toy_simple_io(char **args);
int toy_set_motor_1_speed(char **args);
int toy_set_motor_2_speed(char **args);
int toy_send_to_be(char **args);
int toy_exit(char **args);

char *builtin_str[] = {
    "send",
    "mu",
    "sh",
    "mq",
    "elf",
    "dump",
    "mincore",
    "busy",
    "n",
    "m1",
    "m2",
    "be",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &toy_send,
    &toy_mutex,
    &toy_shell,
    &toy_message_queue,
    &toy_read_elf_header,
    &toy_dump_state,
    &toy_mincore,
    &toy_busy,
    &toy_simple_io,
    &toy_set_motor_1_speed,
    &toy_set_motor_2_speed,
    &toy_send_to_be,
    &toy_exit
};

int toy_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

int toy_send(char **args)
{
    printf("send message: %s\n", args[1]);

    return 1;
}

int toy_mutex(char **args)
{
    if (args[1] == NULL) {
        return 1;
    }

    printf("save message: %s\n", args[1]);
    pthread_mutex_lock(&global_message_mutex);
    strcpy(global_message, args[1]);
    pthread_mutex_unlock(&global_message_mutex);
    return 1;
}

int toy_message_queue(char **args)
{
    int mqretcode;
    toy_msg_t msg;

    if (args[1] == NULL || args[2] == NULL) {
        return 1;
    }

    if (!strcmp(args[1], "camera")) {
        msg.msg_type = atoi(args[2]);
        msg.param1 = 0;
        msg.param2 = 0;
        mqretcode = mq_send(camera_queue, (char *)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);
    }

    return 1;
}

int toy_read_elf_header(char **args)
{
    int mqretcode;
    toy_msg_t msg;
    int in_fd;
    char *contents = NULL;
    size_t contents_sz;
    struct stat st;
    Elf64Hdr *map;

    in_fd = open("./sample/sample.elf", O_RDONLY);
	if ( in_fd < 0 ) {
        printf("cannot open ./sample/sample.elf\n");
        return 1;
    }
    /* 여기서 mmap을 이용하여 파일 내용을 읽으세요.
     * fread 사용 X
     */

    if (!fstat(in_fd, &st)) {
        contents_sz = st.st_size;
        if (!contents_sz) {
            printf("./sample/sample.elf is empty\n");
            return 1;
        }
        printf("real size: %ld\n", contents_sz);
        map = (Elf64Hdr *)mmap(NULL, contents_sz, PROT_READ, MAP_PRIVATE, in_fd, 0);
        printf("Object file type : %d\n", map->e_type);
        printf("Architecture : %d\n", map->e_machine);
        printf("Object file version : %d\n", map->e_version);
        printf("Entry point virtual address : %ld\n", map->e_entry);
        printf("Program header table file offset : %ld\n", map->e_phoff);
        munmap(map, contents_sz);
    }

    return 1;
}

int toy_dump_state(char **args)
{
    int mqretcode;
    toy_msg_t msg;

    msg.msg_type = DUMP_STATE;
    msg.param1 = 0;
    msg.param2 = 0;
    mqretcode = mq_send(camera_queue, (char *)&msg, sizeof(msg), 0);
    assert(mqretcode == 0);
    mqretcode = mq_send(monitor_queue, (char *)&msg, sizeof(msg), 0);
    assert(mqretcode == 0);

    return 1;
}

int toy_mincore(char **args)
{
    unsigned char vec[20];
    int res;
    size_t page = sysconf(_SC_PAGESIZE);
    void *addr = mmap(NULL, 20 * page, PROT_READ | PROT_WRITE,
                    MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    res = mincore(addr, 10 * page, vec);
    assert(res == 0);

    return 1;
}

int toy_busy(char **args)
{
    while (1)
        ;
    return 1;
}

int toy_simple_io(char **args)
{
    int dev;
    char buff = 2;

    dev = open("/dev/toy_simple_io_driver", O_RDWR | O_NDELAY);
	if (dev < 0 ) {
        printf("module open error\n");
        return 1;
    }

    if (args[1] != NULL && !strcmp(args[1], "exit")) {
        buff = 2;
        if (write(dev, &buff, 1) < 0) {
            printf("write error\n");
            goto err;
        }
    } else {
        buff = 1;
        if (write(dev, &buff, 1) < 0) {
            printf("write error\n");
            goto err;
        }
    }

err:
    close(dev);

    return 1;
}

int toy_set_motor_1_speed(char **args)
{
    int dev;
    char buff = 2;
    int speed;

    if (args[1] == NULL) {
        return 1;
    }

    speed = atoi(args[1]);

    if (speed > 100) {
        speed = 100;
    }

    dev = open("/dev/toy_engine_driver", O_RDWR | O_NDELAY);
	if (dev < 0 ) {
        printf("module open error\n");
        return 1;
    }

    if (ioctl(dev, MOTOR_1_SET_SPEED, &speed) < 0) {
        printf("Error while ioctl READ_USE_COUNT (before) in main\n");
        goto err;
    }

err:
    close(dev);

    return 1;
}

int toy_set_motor_2_speed(char **args)
{
    int dev;
    char buff = 2;
    int speed;

    if (args[1] == NULL) {
        return 1;
    }

    speed = atoi(args[1]);

    if (speed > 100) {
        speed = 100;
    }

    dev = open("/dev/toy_engine_driver", O_RDWR | O_NDELAY);
	if (dev < 0 ) {
        printf("module open error\n");
        return 1;
    }

    if (ioctl(dev, MOTOR_2_SET_SPEED, &speed) < 0) {
        printf("Error while ioctl READ_USE_COUNT (before) in main\n");
        goto err;
    }

err:
    close(dev);

    return 1;
}

int toy_send_to_be(char **args)
{
    int mqretcode;
    robot_message_t robot_msg;

    if (args[1] == NULL) {
        return 1;
    }
    robot_msg.id = MESSAGE_ID_INFO;
    robot_msg.data.info.id = 12341234;
    robot_msg.data.info.temperature = atoi(args[1]);
    mqretcode = mq_send(backend_queue, (char *)&robot_msg, sizeof(robot_msg), 0);
    assert(mqretcode == 0);

    return 1;
}

int toy_exit(char **args)
{
    return 0;
}

int toy_shell(char **args)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("toy");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("toy");
    } else {
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int toy_execute(char **args)
{
    int i;

    if (args[0] == NULL) {
        return 1;
    }

    for (i = 0; i < toy_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return 1;
}

char *toy_read_line(void)
{
    char *line = NULL;
    ssize_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror(": getline\n");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **toy_split_line(char *line)
{
    int bufsize = TOY_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token, **tokens_backup;

    if (!tokens) {
        fprintf(stderr, "toy: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOY_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += TOY_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens) {
                free(tokens_backup);
                fprintf(stderr, "toy: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOY_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

void toy_loop(void)
{
    char *line;
    char **args;
    int status;

    do {
        printf("TOY>");
        line = toy_read_line();
        args = toy_split_line(line);
        status = toy_execute(args);
        free(line);
        free(args);
    } while (status);
}

void *command_thread(void* arg)
{
    char *s = arg;

    printf("%s", s);

    toy_loop();

    return 0;
}

int input()
{
    int retcode;
    struct sigaction sa;
    pthread_t command_thread_tid, sensor_thread_tid;
    int i;
    scmp_filter_ctx ctx;

    printf("나 input 프로세스!\n");

    memset(&sa, 0, sizeof(sigaction));
    sigemptyset(&sa.sa_mask);

    // 여기에 seccomp 을 이용해서 mincore 시스템 콜을 막아 주세요.
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL) {
        printf("seccomp_init failed");
        return -1;
    }

    int rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(mincore), 0);
    if (rc < 0) {
        printf("seccomp_rule_add failed");
        return -1;
    }

    seccomp_export_pfc(ctx, 5);
    seccomp_export_bpf(ctx, 6);

    rc = seccomp_load(ctx);
    if (rc < 0) {
        printf("seccomp_load failed");
        return -1;
    }
    seccomp_release(ctx);

    /* 센서 정보를 공유하기 위한, 시스템 V 공유 메모리를 생성한다 */
    the_sensor_info = (shm_sensor_t *)toy_shm_create(SHM_KEY_SENSOR, sizeof(shm_sensor_t));
    if ( the_sensor_info == (void *)-1 ) {
        the_sensor_info = NULL;
        printf("Error in shm_create SHMID=%d SHM_KEY_SENSOR\n", SHM_KEY_SENSOR);
    }

    /* 메시지 큐를 오픈 한다.
     * 하지만, 사실 fork로 생성했기 때문에 파일 디스크립터 공유되었음. 따따서, extern으로 사용 가능
    */
    watchdog_queue = mq_open("/watchdog_queue", O_RDWR);
    assert(watchdog_queue != -1);
    monitor_queue = mq_open("/monitor_queue", O_RDWR);
    assert(monitor_queue != -1);
    disk_queue = mq_open("/disk_queue", O_RDWR);
    assert(disk_queue != -1);
    camera_queue = mq_open("/camera_queue", O_RDWR);
    assert(camera_queue != -1);
    backend_queue = mq_open("/mq_sys_to_be", O_RDWR);
    assert(backend_queue != -1);

   /* 여기서 스레드를 생성한다. */
    retcode = pthread_create(&command_thread_tid, NULL, command_thread, "command thread\n");
    assert(retcode == 0);
    retcode = pthread_create(&sensor_thread_tid, NULL, sensor_thread, "sensor thread\n");
    assert(retcode == 0);

    while (1) {
        sleep(1);
    }

    return 0;
}

int create_input()
{
    pid_t systemPid;
    const char *name = "input";

    printf("여기서 input 프로세스를 생성합니다.\n");

    /* fork 를 이용하세요 */
    switch (systemPid = fork()) {
    case -1:
        printf("fork failed\n");
    case 0:
        /* 프로세스 이름 변경 */
        if (prctl(PR_SET_NAME, (unsigned long) name) < 0)
            perror("prctl()");
        input();
        break;
    default:
        break;
    }

    return 0;
}
