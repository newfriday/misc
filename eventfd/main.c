#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <unistd.h>

int efd;

void *threadFunc()
{
    uint64_t buffer;
    int rc;
    int i = 0;
    while(i++ < 2){
        /* 如果计数器为0，read失败，要么阻塞，要么返回EAGAIN */
        rc = read(efd, &buffer, sizeof(buffer));

        if (rc == 8) {
            printf("notify success, eventfd counter = %lu\n", buffer);
        } else {
            perror("read");
        }
    }
}

static void
open_eventfd(unsigned int initval, int flags)
{
    efd = eventfd(initval, flags);
    if (efd == -1) {
        perror("eventfd");
    }
}

static void
close_eventfd(int fd)
{
    close(fd);
}

static void test(int counter)
{
    int rc;
    pthread_t tid;
    void *status;
    int i = 0;
    uint64_t buf = 2;

    /* create thread */
    if(pthread_create(&tid, NULL, threadFunc, NULL) < 0){
        perror("pthread_create");
    }

    while(i++ < counter){
        rc = write(efd, &buf, sizeof(buf));
        printf("signal to subscriber success, value = %lu\n", buf);

        if(rc != 8){
            perror("write");
        }
        sleep(2);
    }

    pthread_join(tid, &status);
}

int main()
{
    unsigned int initval;

    printf("NON-SEMAPHORE BLOCK way\n");
    /* 初始值为54 flags为0，默认blocking方式读取eventfd */
    initval = 4;
    open_eventfd(initval, 0);
    printf("init counter = %lu\n", initval);

    test(2);

    close_eventfd(efd);

    printf("change to SEMAPHORE way\n");

    /* 初始值为4， 信号量方式维护counter */
    initval = 4;
    open_eventfd(initval, EFD_SEMAPHORE);
    printf("init counter = %lu\n", initval);

    test(2);

    close_eventfd(efd);

    printf("change to NONBLOCK way\n");

    /* 初始值为4， NONBLOCK方式读eventfd */
    initval = 4;
    open_eventfd(initval, EFD_NONBLOCK);
    printf("init counter = %lu\n", initval);

    test(2);

    close_eventfd(efd);

    return 0;
}
