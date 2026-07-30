#ifndef _LINUX_STRING_H_
#define _LINUX_STRING_H_

#include <linux/types.h>	/* for size_t */
#include <linux/stddef.h>	/* for NULL */

#ifdef __cplusplus
extern "C" {
#endif

extern char * ___strtok;
extern char * strpbrk(const char *,const char *);
extern char * strtok(char *,const char *);
extern char * strsep(char **,const char *);
extern __kernel_size_t strspn(const char *,const char *);


/*
 * Include machine specific inline routines
 */
#include <asm/string.h>

#ifndef __HAVE_ARCH_BCOPY
char *bcopy(const char *src, char *dest, int count);
#endif

#ifndef __HAVE_ARCH_STRCPY
extern char * strcpy(char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRNCPY
extern char * strncpy(char *,const char *, __kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif
#ifndef __HAVE_ARCH_STRCAT
extern char * strcat(char *, const char *);
#endif
#ifndef __HAVE_ARCH_STRNCAT
extern char * strncat(char *, const char *, __kernel_size_t);
#endif
#if defined(CONFIG_HW29765960P0P4000P1000P4X4P4X4P4X4) || \
    defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) || \
    defined(CONFIG_HW29766094P0P256P512P2X2P2X2) || \
    defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2) || \
    defined(CONFIG_HW29766193P0P256P512P2X2P2X2)
#ifndef __HAVE_ARCH_STRLCAT
extern size_t strlcat(char *, const char *, __kernel_size_t);
#endif
#endif
#ifndef __HAVE_ARCH_STRCMP
extern int strcmp(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRNCMP
extern int strncmp(const char *,const char *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif
#ifndef __HAVE_ARCH_STRNCASECMP
extern int strncasecmp(const char *s1, const char *s2, __kernel_size_t len);
#endif
#ifndef __HAVE_ARCH_STRCHR
extern char * strchr(const char *,int);
#endif
#ifndef __HAVE_ARCH_STRRCHR
extern char * strrchr(const char *,int);
#endif
#include <linux/linux_string.h>
#ifndef __HAVE_ARCH_STRSTR
extern char * strstr(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRLEN
extern __kernel_size_t strlen(const char *);
#endif
#ifndef __HAVE_ARCH_STRNLEN
extern __kernel_size_t strnlen(const char *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRDUP
extern char * strdup(const char *);
#endif
#ifndef __HAVE_ARCH_STRSWAB
extern char * strswab(const char *);
#endif

#ifndef __HAVE_ARCH_MEMSET
extern void * memset(void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCPY
extern void * memcpy(void *,const void *,__kernel_size_t);
#endif
#if defined(CONFIG_HW29765960P0P4000P1000P4X4P4X4P4X4) || \
    defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) || \
    defined(CONFIG_HW29766094P0P256P512P2X2P2X2) || \
    defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2) || \
    defined(CONFIG_HW29766193P0P256P512P2X2P2X2)
#ifndef __HAVE_ARCH_MEMSCPY
extern size_t memscpy(void *dest, size_t dst_size, const void *src, size_t copy_size);
#endif
#endif
#ifndef __HAVE_ARCH_MEMMOVE
extern void * memmove(void *,const void *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMSCAN
extern void * memscan(void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCMP
extern int memcmp(const void *,const void *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCHR
extern void * memchr(const void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCHR_INV
void *memchr_inv(const void *, int, size_t);
#endif

unsigned long ustrtoul(const char *cp, char **endp, unsigned int base);
unsigned long long ustrtoull(const char *cp, char **endp, unsigned int base);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_STRING_H_ */
