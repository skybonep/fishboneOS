#include <stdint.h>
#include <stddef.h>
#include "user_syscalls.h"

// Simple init program for fishboneOS
// Runs as PID 1, reads config, launches applications

#define MAX_LINE 256
#define MAX_ARGS 16
#define MAX_APPS 8

typedef struct
{
    char *argv[MAX_ARGS];
    int argc;
} app_config_t;

static int read_config_file(const char *filename, app_config_t *apps, int max_apps)
{
    int fd = open(filename);
    if (fd < 0)
    {
        write(2, "init: cannot open config file\n", 31);
        return -1;
    }

    char buffer[4096];
    int bytes_read = read_file(fd, buffer, sizeof(buffer));
    close(fd);

    if (bytes_read <= 0)
    {
        write(2, "init: cannot read config file\n", 30);
        return -1;
    }

    buffer[bytes_read] = '\0'; // Null terminate

    // Simple config parser: each non-empty, non-comment line is "program arg1 arg2 ..."
    int app_count = 0;
    char *line = buffer;

    while (*line && app_count < max_apps)
    {
        // Skip whitespace
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r')
            line++;

        if (*line == '\0')
            break;

        // Skip comments
        if (*line == '#')
        {
            while (*line && *line != '\n')
                line++;
            continue;
        }

        // Find end of line
        char *line_end = line;
        while (*line_end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        char saved_char = *line_end;
        *line_end = '\0';

        // Parse arguments from this line
        apps[app_count].argc = 0;
        char *token = line;

        while (*token && apps[app_count].argc < MAX_ARGS)
        {
            // Skip whitespace
            while (*token == ' ' || *token == '\t')
                token++;

            if (*token == '\0')
                break;

            // Store argument
            apps[app_count].argv[apps[app_count].argc++] = token;

            // Find end of token
            while (*token && *token != ' ' && *token != '\t')
                token++;

            if (*token)
                *token++ = '\0';
        }

        if (apps[app_count].argc > 0)
        {
            app_count++;
        }

        // Restore character and move to next line
        *line_end = saved_char;
        line = line_end;
        if (*line)
            line++;
    }

    return app_count;
}

static void log_message(const char *msg)
{
    write(2, "init: ", 6);
    // Calculate string length
    int len = 0;
    const char *p = msg;
    while (*p++)
        len++;
    write(2, msg, len);
    write(2, "\n", 1);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    log_message("starting init system");

    // Read configuration
    app_config_t apps[MAX_APPS];
    int app_count = read_config_file("/init.cfg", apps, MAX_APPS);

    if (app_count < 0)
    {
        log_message("failed to read config, starting shell");
        // Fallback: just start a shell
        const char *shell_argv[] = {"shell", NULL};
        exec("/shell", shell_argv, NULL);
        log_message("failed to start shell");
        exit(1);
    }

    if (app_count == 0)
    {
        log_message("no applications configured, starting shell");
        const char *shell_argv[] = {"shell", NULL};
        exec("/shell", shell_argv, NULL);
        exit(1);
    }

    log_message("read config, launching applications");

    // Launch applications
    for (int i = 0; i < app_count; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Child process
            log_message("launching application");
            exec(apps[i].argv[0], (const char *const *)apps[i].argv, NULL);
            // If exec fails, exit
            log_message("exec failed");
            exit(1);
        }
        else if (pid < 0)
        {
            log_message("fork failed");
        }
    }

    // Main init loop: monitor children
    log_message("monitoring children");

    while (1)
    {
        int status;
        pid_t pid = wait(-1, &status); // Wait for any child

        if (pid > 0)
        {
            // Child exited, log it
            log_message("child process exited");
        }

        // In a real init system, we might restart critical services here
        sleep(100); // Small delay to prevent busy waiting
    }

    // Should never reach here
    return 0;
}