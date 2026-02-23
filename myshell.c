// This is the code skeleton for Lab 2. If you do not need it, you may also build
/*
Lab 2 – A Simple Shell
Requirements (Refer to the document for accuracy and details):
- internal commands: cd, clr, dir, environ, echo, help, pause, quit
- external commands: fork + exec
- redirection: <, >, >> (stdout redirection also for internal:
dir/environ/echo/help)
- background: '&' at end => do not wait
- batch input: myshell batchfile (EOF => exit)
- env:
shell=<fullpath>/myshell (set in main)
parent=<fullpath>/myshell (set in child before exec)
- prompt includes current directory
*/

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h> //directory command
#include <sys/types.h>
#include <sys/wait.h>
#define MAX_LINE 1024
// Other functions needed ...
extern char **environ; // null terminated array of strings

static void reap_zombies(void)
{
    // Reap any finished background children so you don’t get zombies
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        // keep reaping
    }
}
static int process_line(char *line)
{
    // Skip empty lines
    char *cmd = strtok(line, " \t\n");
    if (cmd == NULL)
    {
        return 1;
    }

    // Parse arguments + redirection tokens
    char *args[64];
    int argc = 0;

    char *infile = NULL;
    char *outfile = NULL;
    int append = 0;
    int background = 0;

    args[argc++] = cmd;

    char *token;
    while ((token = strtok(NULL, " \t\n")) != NULL && argc < 63)
    {
        if (strcmp(token, "<") == 0)
        {
            infile = strtok(NULL, " \t\n");
            if (infile == NULL)
            {
                fprintf(stderr, "Error: missing input file after '<'\n");
                return 1;
            }
        }
        else if (strcmp(token, ">>") == 0)
        {
            outfile = strtok(NULL, " \t\n");
            if (outfile == NULL)
            {
                fprintf(stderr, "Error: missing output file after '>>'\n");
                return 1;
            }
            append = 1;
        }
        else if (strcmp(token, ">") == 0)
        {
            outfile = strtok(NULL, " \t\n");
            if (outfile == NULL)
            {
                fprintf(stderr, "Error: missing output file after '>'\n");
                return 1;
            }
            append = 0;
        }
        else
        {
            args[argc++] = token;
        }
    }
    args[argc] = NULL;

    if (argc > 0 && strcmp(args[argc - 1], "&") == 0)
    {
        background = 1;
        args[argc - 1] = NULL;
        argc--;
    }

    // Built-in commands with OPTIONAL stdout redirection (>, >>)
    int saved_stdout = -1;
    int out_fd = -1;

    int is_internal =
        (strcmp(cmd, "cd") == 0) ||
        (strcmp(cmd, "clr") == 0) ||
        (strcmp(cmd, "dir") == 0) ||
        (strcmp(cmd, "environ") == 0) ||
        (strcmp(cmd, "pause") == 0) ||
        (strcmp(cmd, "echo") == 0) ||
        (strcmp(cmd, "help") == 0);

    int internal_needs_stdout_redir =
        (strcmp(cmd, "dir") == 0) ||
        (strcmp(cmd, "environ") == 0) ||
        (strcmp(cmd, "echo") == 0) ||
        (strcmp(cmd, "help") == 0);

    if (is_internal && internal_needs_stdout_redir && outfile != NULL)
    {
        saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout < 0)
        {
            perror("dup");
            return 1;
        }

        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        out_fd = open(outfile, flags, 0644);
        if (out_fd < 0)
        {
            perror("open");
            close(saved_stdout);
            return 1;
        }

        if (dup2(out_fd, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            close(out_fd);
            close(saved_stdout);
            return 1;
        }
    }

    // cd command
    if (strcmp(cmd, "cd") == 0)
    {
        char *path = args[1];
        if (path == NULL)
        {
            printf("cd: missing argument\n");
        }
        else if (chdir(path) != 0)
        {
            perror("cd");
        }

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // clr command
    if (strcmp(cmd, "clr") == 0)
    {
        printf("\033[2J\033[H");
        fflush(stdout);

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // dir command
    if (strcmp(cmd, "dir") == 0)
    {
        char *path = args[1];
        if (path == NULL)
        {
            path = ".";
        }

        DIR *d = opendir(path);
        if (d == NULL)
        {
            perror("dir");
            if (saved_stdout != -1)
            {
                fflush(stdout);
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
                close(out_fd);
            }
            return 1;
        }

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL)
        {
            printf("%s\n", entry->d_name);
        }

        closedir(d);

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // environ command
    if (strcmp(cmd, "environ") == 0)
    {
        char **env = environ;
        while (*env != NULL)
        {
            printf("%s\n", *env);
            env++;
        }

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // echo command
    if (strcmp(cmd, "echo") == 0)
    {
        for (int i = 1; args[i] != NULL; i++)
        {
            printf("%s", args[i]);
            if (args[i + 1] != NULL)
            {
                printf(" ");
            }
        }
        printf("\n");

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // help command
    if (strcmp(cmd, "help") == 0)
    {
        system("more readme");

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // pause command
    if (strcmp(cmd, "pause") == 0)
    {
        printf("Shell paused. Press ENTER to continue...\n");

        int c;
        while ((c = getchar()) != '\n' && c != EOF)
        {
        }

        if (saved_stdout != -1)
        {
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            close(out_fd);
        }
        return 1;
    }

    // External command execution
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    else if (pid == 0)
    {
        // child process - handle I/O redirection
        if (infile)
        {
            int fd_in = open(infile, O_RDONLY);
            if (fd_in < 0)
            {
                perror("open infile");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        if (outfile)
        {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd_out = open(outfile, flags, 0644);
            if (fd_out < 0)
            {
                perror("open outfile");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        char *shell_path = getenv("shell");
        if (shell_path != NULL)
        {
            setenv("parent", shell_path, 1);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(1);
    }
    else
    {
        // parent process
        if (!background)
        {
            int status;
            waitpid(pid, &status, 0);
        }
        else
        {
            // do not wait: child will be reaped by reap_zombies()
        }
    }

    return 1;
}

int main(int argc, char *argv[])
{
    // TODO: set environment variable "shell" to the full path of myshell
    FILE *in = stdin;
    if (argc == 2)
    {
        in = fopen(argv[1], "r");
        if (!in)
        {
            perror("fopen");
            return 1;
        }
    }
    else if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [batchfile]\n", argv[0]);
        return 1;
    }
    char line[MAX_LINE];
    char cwd[PATH_MAX];

    // =========================
    // Shell environment setup
    // =========================

    char resolved_path[PATH_MAX];

    if (realpath(argv[0], resolved_path) != NULL)
    {
        setenv("shell", resolved_path, 1);
    }
    else
    {
        perror("realpath failed");
    }

    while (1)
    {
        reap_zombies();
        if (in == stdin)
        {
            if (getcwd(cwd, sizeof(cwd)) == NULL)
            {
                perror("getcwd");
                break;
            }
            printf(">myshell:%s$ ", cwd);
            fflush(stdout);
        }
        if (!fgets(line, sizeof(line), in))
            break; // EOF => exit (batch requirement)
        if (process_line(line) == 0)
            break;
    }
    if (in != stdin)
        fclose(in);
    return 0;
}

void print_environ()
{
    char **env = environ;

    while (*env != NULL)
    {
        printf("%s\n", *env);
        env++;
    }
}
/*
========================
Guidance / Hints
Content: Chenyu NING
Email: chenyu.ning@ontariotechu.ca
Format: chatGPT (Be an honest person)
*This section contains function descriptions intended to guide you when
implementing the corresponding features.
*If you do not need this guidance, you may delete it directly.
*Based on these descriptions, you should be able to complete this assignment
independently.
========================
--------------------------------------------------
1.i cd
--------------------------------------------------
fgets(buffer, size, stdin)
- reads an entire line from standard input stdin (until '\n' or EOF)
- includes the '\n' character in buffer
- returns buffer on success, NULL on failure
- used in this shell to read a complete command entered by the user
strtok(char *str, const char *delim)
- used to split a string using delimiters
- *modifies the original string* (by inserting '\0')
- this lab assumes all arguments are separated by spaces or tabs
strcmp(str1, str2)
- compares two strings
- returns 0 if they are exactly the same
getcwd(char *buf, size)
- retrieves the current working directory
- returns buf on success, NULL on failure
--------------------------------------------------
1.ii clr
--------------------------------------------------
fflush(stdout)
- forcibly flushes the stdout buffer
- ensures output is immediately displayed in the terminal
ANSI escape sequences
- terminals interpret certain strings beginning with '\033' as control commands
- commonly used for clearing the screen, moving the cursor, changing colors,
etc.
Common examples:
\033[2J clear the screen
\033[H move cursor to the top-left corner
\033[K clear from cursor to end of line
\033[2K clear the entire line
\033[row;colH move cursor to the specified row and column
\033[31m red text
\033[0m reset styles
--------------------------------------------------
1.iii dir
--------------------------------------------------
strcat(dest, src)
- appends src to the end of dest
- requires dest to have sufficient space
DIR *opendir(const char *path)
- opens a directory
- returns DIR* on success, NULL on failure
struct dirent *readdir(DIR *dir)
- reads an entry from the directory
- returns a struct dirent* containing the filename and other information
- returns NULL when no more entries are available
closedir(DIR *dir)
- closes the directory stream and releases resources
--------------------------------------------------
1.iv environ - cm
--------------------------------------------------
extern char **environ
- a global variable provided by the C runtime
- environ is a NULL-terminated array of strings
- the environ command simply prints this array
--------------------------------------------------
1.vi help
--------------------------------------------------
fork()
- creates a child process
- both parent and child continue execution from the return point of fork
- return values:
* returns 0 in the child process
* returns the child PID (>0) in the parent process
* returns -1 on failure
Why must the shell use fork?
- exec-family functions *replace the current process*
- if the shell directly uses exec to run an external program, the shell itself
will terminate
- correct approach:
shell (parent)
└── fork
└── child -> exec external program
execvp(file, argv)
- replaces the current process image with a new program
- file: program name (searched in PATH)
- argv: argument array, must be NULL-terminated
- does not return on success, returns -1 on failure
wait(&status)
- blocks until any child process terminates
- status stores the child's exit status
waitpid(pid, &status, options)
- waits for a specific child process
- commonly used options:
* 0 : blocking
* WNOHANG : non-blocking (used to reap zombie processes in background
execution)
--------------------------------------------------
1.vii pause
--------------------------------------------------
getchar()
- reads a single character from stdin
- blocks if no input is available (used for pause)
--------------------------------------------------
1.ix shell environment
--------------------------------------------------
realpath(path, resolved_path)
- resolves a relative path into an absolute path
- used to set shell=<absolute_path>/myshell
setenv(name, value, overwrite)
- sets an environment variable
- overwrite=1 allows an existing value to be replaced
getenv(name)
- retrieves the value of an environment variable
- returns NULL if it does not exist
--------------------------------------------------
3. external command
--------------------------------------------------
fopen(filename, mode)
- opens a file and returns a FILE*
- common modes:
"r" read-only (error if file does not exist)
"w" write (create if not exists, truncate if exists)
"a" append (create if not exists)
FILE* vs file descriptor
- FILE* is an abstraction provided by the C standard library
- internally, it still interacts with the OS using file descriptors (int)
- fileno(FILE*) retrieves the corresponding file descriptor
--------------------------------------------------
4. i/o-redirection
--------------------------------------------------
dup(fd)
- duplicates a file descriptor
- returns a new fd pointing to the same file
dup2(oldfd, newfd)
- duplicates oldfd onto the specified descriptor number newfd
- if newfd is already open, it is closed first
Core idea of redirection:
- stdout is essentially file descriptor 1
- by redirecting fd=1 to a file, printf will write to that file
Redirection strategy for internal commands:
1. save the current stdout
2. dup2 stdout to the file
3. execute the internal command
4. restore stdout
Redirection strategy for external commands:
- modify stdin/stdout in the child process
- exec the external program
- this does not affect the parent shell
--------------------------------------------------
5. background execution
--------------------------------------------------
Zombie process
- the child process has terminated
- the parent process has not called wait
- if the parent directly returns to the main loop instead of calling waitpid,
it effectively implements background execution
waitpid(-1, NULL, WNOHANG)
- does not block the process
- reaps any terminated child process
- returns immediately if no child process has terminated
*/
