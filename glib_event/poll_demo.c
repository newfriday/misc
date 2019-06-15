#include    <poll.h>
#include    <stdio.h>
#include    <fcntl.h>

int main()
{
    struct pollfd  fda;
    int retval;
    fda.fd = open("poll_test", O_RDWR | O_CREAT);
    if (fda.fd < 0)
    {
        perror("open()");
        return -1;
    }

    fda.events = POLLIN | POLLRDNORM  ;

    retval = poll(&fda, 1, -1);
    if ( retval == 0)
        printf("timeout return\n");
    else
    {
        if (fda.revents & POLLRDNORM)
            printf("ready to read\n");
        if (fda.revents & POLLWRNORM)
            printf("ready to write\n");
    }
}
