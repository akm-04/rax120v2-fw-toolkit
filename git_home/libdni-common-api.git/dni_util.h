#ifndef _DNI_UTIL_H
#define _DNI_UTIL_H
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

extern void __nprintf(const char *fmt, ...);
extern int dni_fscanf(FILE *stream,const char * format,...);
extern int dni_snprintf(char *s,size_t maxlen,const char *format,...);
extern char * dni_fgets(char *s,int count,FILE *stream);
extern size_t dni_strlcpy(char *dst,const char *src,size_t siz);
extern size_t dni_strlcat(char *dst,const char *src,size_t siz);
extern char *dni_strncat(char *dest, const char *src, size_t n);
extern int dni_strcmp(const char *p1,const char *p2);
extern int dni_strncmp(const char *p1,const char *p2,size_t siz);
extern size_t dni_strlen(const char *str);
extern int dni_strcasecmp(const char *s1, const char *s2);
extern char *dni_strstr(const char* s1, const char *s2);
extern char *dni_strcasestr(const char* s1, const char *s2);
extern int dni_atoi(const char *nptr);
extern char *dni_strdup(char *str);
extern char *dni_itoa(int num);
extern char *dni_strncpy(char *dest, const char *src, size_t n);
#endif
