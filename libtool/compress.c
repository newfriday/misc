#include <stdio.h>
#include <stdlib.h>   // EXIT_FAILURE
#include <dlfcn.h>    // dlopen, dlerror, dlsym, dlclose

typedef int(* FUNC_ZLIB)(const char* filename); // 定义函数指针类型的别名
const char* dllPath = "./libadd.so";

int compress(const char* filename)
{
    void *handle = NULL;
    int flags = RTLD_NOW | RTLD_GLOBAL;
    FUNC_ZLIB zlib = NULL;
    int ret;

    if (!(handle = dlopen(filename, flags))) {
        fprintf(stderr, "Failed to load module '%s': %s", filename, dlerror());
        return -1;
    }

    zlib = (FUNC_ZLIB)dlsym(handle, "zlib");

    ret = zlib(filename);

    dlclose(handle);

    return ret;
}
