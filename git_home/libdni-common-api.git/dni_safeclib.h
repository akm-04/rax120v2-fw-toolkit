#ifndef __DNI_SAFECLIB__
#define __DNI_SAFECLIB__

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "safe_types.h"
#include "safe_str_lib.h"
#include "dni_util.h"

static char *funcName;
static size_t lineno;

#define DNI_strcpy_s(dest, dmax, src) assert(dni_strcpy_s(__FUNCTION__, __LINE__, (dest), (dmax), (src)) != ESNULLP)
#define DNI_memcpy_s(dest, dmax, src, smax) assert(dni_memcpy_s(__FUNCTION__, __LINE__, (dest), (dmax), (src), (smax)) != ESNULLP)
#define DNI_sprintf_s(dest, dmax, fmt, ...) dni_sprintf_s(__FUNCTION__, __LINE__,(dest), (dmax), (fmt), ##__VA_ARGS__)
#define DNI_memset_s(dest, dmax, value, count) assert(dni_memset_s(__FUNCTION__, __LINE__,(dest), (dmax), (value), (count)) != ESNULLP)
#define DNI_strcasecmp_s(dest, dmax, src) dni_strcasecmp_s(__FUNCTION__, __LINE__,(dest), (dmax), (src))
#define DNI_strcat_s(dest, dmax, src) assert(dni_strcat_s(__FUNCTION__, __LINE__,(dest), (dmax), (src)) != ESNULLP)
#define DNI_strncat_s(dest, dmax, src, slen) assert(dni_strncat_s(__FUNCTION__, __LINE__,(dest), (dmax), (src), (slen)) != ESNULLP)
#define DNI_strncpy_s(dest, dmax, src, slen) assert(dni_strncpy_s(__FUNCTION__, __LINE__,(dest), (dmax), (src), (slen)) != ESNULLP)
#define DNI_strcmp_s(dest, dmax, src) dni_strcmp_s(__FUNCTION__, __LINE__,(dest), (dmax), (src))
#define DNI_strstr_s(dest, dmax, src, slen) dni_strstr_s(__FUNCTION__, __LINE__,(dest), (dmax), (src), (slen))
#define DNI_strcasestr_s(dest, dmax, src, slen) dni_strcasestr_s(__FUNCTION__, __LINE__, (dest), (dmax), (src), (slen))
#define DNI_strchr_s(dest, dmax, ch) dni_strchr_s(__FUNCTION__, __LINE__, (dest), (dmax), (ch))
#define DNI_strrchr_s(dest, dmax, ch) dni_strchr_s(__FUNCTION__, __LINE__, (dest), (dmax), (ch))
//#define DNI_strtok_s(dest, dmax, src, ptr) dni_strtok_s(__FUNCTION__, __LINE__, (dest), (dmax), (src), (ptr)) 
#define DNI_fopen_s(pFile, filename, mode) assert(dni_fopen_s(__FUNCTION__, __LINE__, (pFile), (filename), (mode)) != ESNULLP) 
#define DNI_sscanf_s(dest, fmt, ...) dni_sscanf_s(__FUNCTION__, __LINE__, (dest), (fmt), ##__VA_ARGS__)
#define DNI_fscanf_s(stream, fmt, ...) dni_fscanf_s(__FUNCTION__, __LINE__, (stream), (fmt), ##__VA_ARGS__)
#define DNI_fprintf_s(stream, fmt, ...) dni_fprintf_s(__FUNCTION__, __LINE__, (stream), (fmt), ##__VA_ARGS__)
#define DNI_memmove_s(dest, dmax, src, smax) assert(dni_memmove_s(__FUNCTION__, __LINE__, (dest), (dmax), (src), (smax)) != ESNULLP)
#define DNI_strnlen_s(dest, dmax) dni_strnlen_s(__FUNCTION__, __LINE__, (dest), (dmax))

/****************************C11 APIs declaration***************************/
void set_log_function_line(const char *func, size_t line);
void dni_handle_str_errors(const char *msg, void *ptr, errno_t error) ;
void dni_handle_mem_errors(const char *msg, void *ptr, errno_t error) ;
constraint_handler_t  dni_set_str_constraint_handler_s(constraint_handler_t handler);
constraint_handler_t  dni_set_mem_constraint_handler_s(constraint_handler_t handler);
errno_t dni_strcpy_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src);
errno_t dni_memcpy_s(const char *func, size_t line, void *dest, rsize_t dmax, const void *src, rsize_t count);
int dni_sprintf_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *fmt, ...);
errno_t dni_memset_s(const char *func, size_t line, void *dest, rsize_t dmax, int value, rsize_t count);
int dni_strcasecmp_s(const char *func, size_t line, const char *dest, rsize_t dmax, const char *src);
errno_t dni_strcat_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src);
errno_t dni_strncat_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src, rsize_t slen);
errno_t dni_strncpy_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src, rsize_t slen);
int dni_strcmp_s(const char *func, size_t line, const char *dest, rsize_t dmax, const char *src);
char *dni_strstr_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src, rsize_t slen);
char *dni_strcasestr_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src, rsize_t slen);
char *dni_strchr_s(const char *func, size_t line, const char *dest, rsize_t dmax, const int ch);
char *dni_strrchr_s(const char *func, size_t line, const char *dest, rsize_t dmax, const int ch);
//char *dni_strtok_s(const char *func, size_t line, char *dest, rsize_t dmax, const char *src, char *ptr);
errno_t dni_fopen_s(const char *func, size_t line, FILE **pFile, const char *filename, const char *mode);
int dni_sscanf_s(const char *func, size_t line, const char *dest, const char *fmt, ...);
int dni_fscanf_s(const char *func, size_t line, FILE *stream, const char *fmt, ...);
int dni_fprintf_s(const char *func, size_t line, FILE *stream, const char *fmt, ...);
errno_t dni_memmove_s(const char *func, size_t line, void *dest, rsize_t dmax, const void *src, rsize_t count);
size_t dni_strnlen_s(const char *func, size_t line, const char *dest, rsize_t dmax);



/*******************************Other Common feature API declaration************************/
unsigned int DNI_random();

#endif

