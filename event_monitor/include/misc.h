#ifndef __GEM_STRING_H__
# define __GEM_STRING_H__

char *gemStrncpy(char *dest, const char *src, size_t n, size_t destbytes);
char *gemStrcpy(char *dest, const char *src, size_t destbytes);

# define gemStrcpyStatic(dest, src) gemStrcpy((dest), (src), sizeof(dest))

int gemReallocN(void *ptrptr, size_t size, size_t count);

/**
 * GEM_REALLOC_N:
 * @ptr: pointer to hold address of allocated memory
 * @count: number of elements to allocate
 *
 * Re-allocate an array of 'count' elements, each sizeof(*ptr)
 * bytes long and store the address of allocated memory in
 * 'ptr'. If 'ptr' grew, the added memory is uninitialized.
 *
 * This macro is safe to use on arguments with side effects.
 *
 * Returns 0 on success, aborts on OOM
 */
#define GEM_REALLOC_N(ptr, count) gemReallocN(&(ptr), sizeof(*(ptr)), (count))

#endif /* __GEM_STRING_H__ */
