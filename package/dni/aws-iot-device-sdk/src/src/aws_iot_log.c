#include "aws_iot_log.h"

void console_printf(const char *fmt, ...)
{
        va_list ap;
        static FILE *filp;

        if ((filp == NULL) && (filp = fopen("/dev/console", "a")) == NULL)
                return;

        va_start(ap, fmt);
        vfprintf(filp, fmt, ap);
        va_end(ap);
}

