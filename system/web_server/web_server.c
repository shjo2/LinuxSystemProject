#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

#define STACK_SIZE (8 * 1024 * 1024)
#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

static int child_func(void *arg)
{
    char path[1024];
    int n;
    struct ifreq ifr;
    char array[] = "eth0";

    n = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , array , IFNAMSIZ - 1);
    ioctl(n, SIOCGIFADDR, &ifr);
    close(n);

    if (getcwd(path, 1024) == NULL) {
        fprintf(stderr, "current working directory get error: %s\n", strerror(errno));
        return -1;
    }

    printf(" - [%4d] Current namspace, Parent PID : %d\n", getpid(), getppid() );
    printf("current working directory: %s\n", path);

    // 파일 시스템 완전 격리를 위해서는 pivot_root 필요
    // https://zbvs.tistory.com/14

     if (execl("./toy-be", "toy-be", "-a", inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr),"-p", "8080", "-i", "12341234", (char *) NULL)) {
         printf("execfailed\n");
     }
}

int create_web_server()
{
    pid_t child_pid;

    printf("여기서 Web Server 프로세스를 생성합니다.\n");

    char *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        printf("mmap failed\n");
        return -1;
    }

    child_pid = clone(child_func, stack + STACK_SIZE, SIGCHLD, "Hello");
    if (child_pid == -1) {;
        errExit("clone\n");
    }

    printf("PID of child created by clone() is %ld\n", (long) child_pid);

    munmap(stack, STACK_SIZE);

    return 0;
}
