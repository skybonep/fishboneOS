#include <stdint.h>
#include <stddef.h>
#include "user_syscalls.h"

// Simple shell for fishboneOS
// Basic command interpreter

#define MAX_CMD 256
#define MAX_ARGS 16

// Simple string comparison (since we don't have libc)
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

static void print_prompt(void)
{
    write(2, "fishbone> ", 10);
}

static int parse_command(char *cmd, char *argv[], int max_args)
{
    int argc = 0;
    char *token = cmd;

    while (*token && argc < max_args)
    {
        // Skip whitespace
        while (*token == ' ' || *token == '\t')
            token++;

        if (*token == '\0')
            break;

        // Store argument
        argv[argc++] = token;

        // Find end of argument
        while (*token && *token != ' ' && *token != '\t')
            token++;

        if (*token)
            *token++ = '\0';
    }

    argv[argc] = NULL;
    return argc;
}

static void execute_command(char *argv[])
{
    if (argv[0] == NULL)
        return;

    // Built-in commands
    if (strcmp(argv[0], "exit") == 0)
    {
        write(2, "Goodbye!\n", 9);
        exit(0);
    }
    else if (strcmp(argv[0], "help") == 0)
    {
        write(2, "Available commands:\n", 19);
        write(2, "  help     - show this help\n", 27);
        write(2, "  exit     - exit shell\n", 23);
        write(2, "  ls       - list files\n", 22);
        write(2, "  cat <file> - display file contents\n", 35);
        write(2, "  <program> - execute program\n", 29);
        return;
    }
    else if (strcmp(argv[0], "ls") == 0)
    {
        // Simple directory listing (not implemented in kernel yet)
        write(2, "Directory listing not implemented\n", 34);
        return;
    }
    else if (strcmp(argv[0], "cat") == 0 && argv[1] != NULL)
    {
        // Display file contents
        int fd = open(argv[1]);
        if (fd < 0)
        {
            write(2, "Cannot open file\n", 17);
            return;
        }

        char buffer[512];
        int bytes_read;
        while ((bytes_read = read_file(fd, buffer, sizeof(buffer))) > 0)
        {
            write(2, buffer, bytes_read);
        }
        close(fd);
        return;
    }

    // Try to execute as external program
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        exec(argv[0], (const char *const *)argv, NULL);
        // If exec fails, exit
        write(2, "Command not found\n", 18);
        exit(1);
    }
    else if (pid > 0)
    {
        // Parent process - wait for child
        int status;
        wait(pid, &status);
    }
    else
    {
        write(2, "Fork failed\n", 12);
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    write(2, "fishboneOS Shell v0.1\n", 22);
    write(2, "Type 'help' for commands\n\n", 27);

    char cmd_buffer[MAX_CMD];
    char *cmd_argv[MAX_ARGS];

    while (1)
    {
        print_prompt();

        // Read command line
        int i = 0;
        int ch;
        while (i < MAX_CMD - 1 && (ch = read(0)) != '\n' && ch != '\r')
        {
            if (ch > 0)
            {
                cmd_buffer[i++] = (char)ch;
                // Echo character
                char echo[2] = {(char)ch, '\0'};
                write(2, echo, 1);
            }
        }
        cmd_buffer[i] = '\0';
        write(2, "\n", 1);

        if (i == 0)
            continue; // Empty line

        // Parse and execute command
        int cmd_argc = parse_command(cmd_buffer, cmd_argv, MAX_ARGS);
        if (cmd_argc > 0)
        {
            execute_command(cmd_argv);
        }
    }

    return 0;
}