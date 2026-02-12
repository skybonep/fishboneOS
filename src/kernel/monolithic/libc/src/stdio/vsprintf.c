#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h> // for strlen

int vsprintf(char *buf, const char *fmt, va_list args) {
    char *ptr = buf;
    const char *f = fmt;

    while (*f != '\0') {
        if (*f != '%') {
            *ptr++ = *f++;
            continue;
        }
        f++; // Move past '%'

        int zero_pad = 0;
        int width = 0;

        // 1. Check for '0' indicating zero-padding
        if (*f == '0') {
            zero_pad = 1;
            f++;
        }

        // 2. Parse the width (e.g., the '8' in %08x)
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }

        char tmp_buf[5]; // Temporary buffer for itoa
        char *t_ptr;

        switch (*f) {
            case 'c': 
                *ptr++ = (char)va_arg(args, int); 
                break;
            case 's': {
                char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                while (*s) *ptr++ = *s++;
                break;
            }
            case 'd':
            case 'x':
            case 'b': {
                int base = (*f == 'd') ? 10 : (*f == 'x' ? 16 : 2);
                itoa(va_arg(args, int), tmp_buf, base);
                
                int len = strlen(tmp_buf);
                if (zero_pad && width > len) {
                    for (int i = 0; i < (width - len); i++) {
                        *ptr++ = '0';
                    }
                }
                
                t_ptr = tmp_buf;
                while (*t_ptr) *ptr++ = *t_ptr++;
                break;
            }
            case '%': 
                *ptr++ = '%'; 
                break;
            default:  
                *ptr++ = *f;   
                break;
        }
        f++;
    }
    *ptr = '\0';
    return (int)(ptr - buf);
}
