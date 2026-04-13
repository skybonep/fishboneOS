#include <stdint.h>
#include <stddef.h>
#include "user_syscalls.h"

// Simple hello world program for testing user-space execution

// Simple string length function (since we don't have libc)
size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    write(2, "Hello, fishboneOS!\n", 19);
    write(2, "This is a user-space program running in the kernel.\n", 53);

    // Print arguments if any
    if (argc > 1)
    {
        write(2, "Arguments: ", 11);
        for (int i = 1; i < argc; i++)
        {
            write(2, argv[i], strlen(argv[i]));
            if (i < argc - 1)
                write(2, " ", 1);
        }
        write(2, "\n", 1);
    }

    return 0;
}