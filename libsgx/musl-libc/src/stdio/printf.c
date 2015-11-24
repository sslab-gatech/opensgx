#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

static
void printchar(char **str, int c)
{
    putchar((char)c);
}

#define PAD_RIGHT 1
#define PAD_ZERO 2

static
int prints(char **out, const char *string, int width, int pad) {
    register int pc = 0, padchar = ' ';

    if (width > 0) {
        register int len = 0;
        register const char *ptr;
        for (ptr = string; *ptr; ++ptr) ++len;
        if (len >= width) width = 0;
        else width -= len;
        if (pad & PAD_ZERO) padchar = '0';
    }
    if (!(pad & PAD_RIGHT)) {
        for ( ; width > 0; --width) {
            printchar (out, padchar);
            ++pc;
        }
    }
    for ( ; *string ; ++string) {
        printchar (out, *string);
        ++pc;
    }
    for ( ; width > 0; --width) {
        printchar (out, padchar);
        ++pc;
    }

    return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 12

static
int printi(char **out, int i, int b, int sg, int width, int pad, int letbase) {
    char print_buf[PRINT_BUF_LEN];
    register char *s;
    register int t, neg = 0, pc = 0;
    register unsigned int u = i;

    if (i == 0) {
        print_buf[0] = '0';
        print_buf[1] = '\0';
        return prints (out, print_buf, width, pad);
    }

    if (sg && b == 10 && i < 0) {
        neg = 1;
        u = -i;
    }

    s = print_buf + PRINT_BUF_LEN-1;
    *s = '\0';

    while (u) {
        t = u % b;
        if ( t >= 10 )
            t += letbase - '0' - 10;
        *--s = t + '0';
        u /= b;
    }

    if (neg) {
        if ( width && (pad & PAD_ZERO) ) {
            printchar (out, '-');
            ++pc;
            --width;
        }
        else {
            *--s = '-';
        }
    }

    return pc + prints (out, s, width, pad);
}

static
int print(char **out, const char *fmt, va_list args) {
    register int width, pad;
    register int pc = 0;
    char scr[2];

    for (; *fmt != 0; ++fmt) {
        if (*fmt == '%') {
            ++fmt;
            width = pad = 0;
            if (*fmt == '\0') break;
            if (*fmt == '%') goto out;
            if (*fmt == '-') {
                ++fmt;
                pad = PAD_RIGHT;
            }
            while (*fmt == '0') {
                ++fmt;
                pad |= PAD_ZERO;
            }
            for ( ; *fmt >= '0' && *fmt <= '9'; ++fmt) {
                width *= 10;
                width += *fmt - '0';
            }
            if (*fmt == 's') {
                register void *s = (char *)(uintptr_t)va_arg( args, int );
                pc += prints (out, s?s:"(null)", width, pad);
                continue;
            }
            if (*fmt == 'd') {
                pc += printi (out, va_arg( args, int ), 10, 1, width, pad, 'a');
                continue;
            }
            if (*fmt == 'x') {
                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'a');
                continue;
            }
            if (*fmt == 'X') {
                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'A');
                continue;
            }
            if (*fmt == 'u') {
                pc += printi (out, va_arg( args, int ), 10, 0, width, pad, 'a');
                continue;
            }
            if (*fmt == 'c') {
                /* char are converted to int then pushed on the stack */
                scr[0] = (char)va_arg( args, int );
                scr[1] = '\0';
                pc += prints (out, scr, width, pad);
                continue;
            }
        }
        else {
out:
            printchar (out, *fmt);
            ++pc;
        }
    }
    if (out) **out = '\0';
    va_end(args);
    return pc;
}


int printf(const char *restrict fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    return print(0, fmt, args);
}
