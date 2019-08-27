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
    while(1){
        rc = read(efd, &buffer, sizeof(buffer));

        if(rc == 8){
            printf("notify success\n");
        }

        printf("rc = %llu, buffer = %lu\n",(unsigned long long)rc, buffer);
    }//end while
}

int main()
{
    pthread_t tid;
    int rc;
    uint64_t buf = 3;

    efd = eventfd(1,0);     // blocking阻塞等待
    if(efd == -1){
        perror("eventfd");
    }

    //create thread
    if(pthread_create(&tid, NULL, threadFunc, NULL) < 0){
        perror("pthread_create");
    }

    while(1){
        rc = write(efd, &buf, sizeof(buf));

        if(rc != 8){
            perror("write");
        }
        sleep(2);
    }//end while
    close(efd);
    return 0;
}

