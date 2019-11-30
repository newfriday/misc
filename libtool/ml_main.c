
int ml_func(int a, int b)
{
    int myglob = 42;
    myglob += a;
    return b + myglob;
}
