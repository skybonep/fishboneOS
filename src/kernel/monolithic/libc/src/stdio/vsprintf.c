#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h> // for strlen

/*
 * vsprintf:
 *   Formats the string described by fmt into the provided buffer.
 *
 * Supported format specifiers:
 *   %c    - character (argument promoted from int)
 *   %s    - null-terminated string pointer
 *   %d    - signed decimal integer
 *   %u    - unsigned decimal integer
 *   %x    - unsigned hexadecimal integer (lowercase)
 *   %b    - unsigned binary integer
 *   %p    - pointer value printed as 0x followed by lowercase hex digits
 *   %%    - literal percent sign
 *
 * Formatting options supported:
 *   0<N>  - zero-padding for numeric output, e.g. %08x
 *   <N>   - minimum field width for numeric output, e.g. %4u
 *
 * Behavior for unsupported specifiers:
 *   The character after '%' is copied verbatim into the output.
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
    char *ptr = buf;
    const char *f = fmt;

    while (*f != '\0')
    {
        if (*f != '%')
        {
            *ptr++ = *f++;
            continue;
        }
        f++; // Move past '%'

        int zero_pad = 0;
        int width = 0;

        // 1. Check for '0' indicating zero-padding
        if (*f == '0')
        {
            zero_pad = 1;
            f++;
        }

        // 2. Parse the width (e.g., the '8' in %08x)
        while (*f >= '0' && *f <= '9')
        {
            width = width * 10 + (*f - '0');
            f++;
        }

        char tmp_buf[34]; // Temporary buffer for integer conversion
        char *t_ptr;

        switch (*f)
        {
        case 'c':
            *ptr++ = (char)va_arg(args, int);
            break;
        case 's':
        {
            char *s = va_arg(args, char *);
            if (!s)
                s = "(null)";
            while (*s)
                *ptr++ = *s++;
            break;
        }
        case 'd':
        {
            int base = 10;
            itoa(va_arg(args, int), tmp_buf, base);

            int len = strlen(tmp_buf);
            if (zero_pad && width > len)
            {
                for (int i = 0; i < (width - len); i++)
                {
                    *ptr++ = '0';
                }
            }

            t_ptr = tmp_buf;
            while (*t_ptr)
                *ptr++ = *t_ptr++;
            break;
        }
        case 'u':
        {
            unsigned int value = va_arg(args, unsigned int);
            uitoa(value, tmp_buf, 10);
            int len = strlen(tmp_buf);
            if (zero_pad && width > len)
            {
                for (int i = 0; i < (width - len); i++)
                {
                    *ptr++ = '0';
                }
            }
            t_ptr = tmp_buf;
            while (*t_ptr)
                *ptr++ = *t_ptr++;
            break;
        }
        case 'x':
        case 'b':
        {
            int base = (*f == 'x') ? 16 : 2;
            uitoa(va_arg(args, unsigned int), tmp_buf, base);

            int len = strlen(tmp_buf);
            if (zero_pad && width > len)
            {
                for (int i = 0; i < (width - len); i++)
                {
                    *ptr++ = '0';
                }
            }

            t_ptr = tmp_buf;
            while (*t_ptr)
                *ptr++ = *t_ptr++;
            break;
        }
        case 'p':
        {
            unsigned int value = (unsigned int)va_arg(args, void *);
            *ptr++ = '0';
            *ptr++ = 'x';
            uitoa(value, tmp_buf, 16);
            int len = strlen(tmp_buf);
            if (zero_pad && width > (len + 2))
            {
                for (int i = 0; i < (width - len - 2); i++)
                {
                    *ptr++ = '0';
                }
            }
            t_ptr = tmp_buf;
            while (*t_ptr)
                *ptr++ = *t_ptr++;
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
