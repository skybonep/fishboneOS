#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int vsprintf(char *buf, const char *fmt, va_list args) {
    char *ptr = buf;
    const char *f = fmt;

    while (*f != '\0') {
        if (*f != '%') {
            *ptr++ = *f++;
            continue;
        }
        f++; // Move past '%'

        switch (*f) {
            case 'c': *ptr++ = (char)va_arg(args, int); break;
            case 's': {
                char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                while (*s) *ptr++ = *s++;
                break;
            }
            case 'd': itoa(va_arg(args, int), ptr, 10); while (*ptr) ptr++; break;
            case 'x': itoa(va_arg(args, int), ptr, 16); while (*ptr) ptr++; break;
            case 'b': itoa(va_arg(args, int), ptr, 2);  while (*ptr) ptr++; break;
            case '%': *ptr++ = '%'; break;
            default:  *ptr++ = *f;   break;
        }
        f++;
    }
    *ptr = '\0';
    return (int)(ptr - buf);
}