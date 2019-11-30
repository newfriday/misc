#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int adder(int a, int b, int c, int d,
          int e, int f, int g, int h);

int adder(int a, int b, int c, int d,
          int e, int f, int g, int h)
{
    return a+b+c+d+e+f+g+h;
}

void main(void)
{
    int sum;
    char *s = malloc(10);
    memcpy(s, "xxxxxxxxx", 9);
    s[9] = '\0';
    sum = adder(1,2,3,4,5,6,7,8);
    printf("s=%s,sum=%d\n",s,sum);
    while(1)
    {}
}


