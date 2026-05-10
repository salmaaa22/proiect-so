#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t got_sigint = 0;

static void write_message(const char *message)
{
    write(STDOUT_FILENO, message, strlen(message));
    write(STDOUT_FILENO, "\n", 1);
}

static void handle_sigusr1(int signum)
{
    (void)signum;
    write_message("monitor_reports: new report notification received");
}

static void handle_sigint(int signum)
{
    (void)signum;
    got_sigint = 1;
    keep_running = 0;
    write_message("monitor_reports: SIGINT received, shutting down");
}

int main(void)
{
    struct sigaction sa_usr1;
    struct sigaction sa_int;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    memset(&sa_int, 0, sizeof(sa_int));

    sa_usr1.sa_handler = handle_sigusr1;
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_usr1.sa_mask);
    sigemptyset(&sa_int.sa_mask);
    sa_usr1.sa_flags = 0;
    sa_int.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("sigaction SIGUSR1");
        return 1;
    }
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open .monitor_pid");
        return 1;
    }

    char pid_buf[32];
    int len = snprintf(pid_buf, sizeof(pid_buf), "%ld\n", (long)getpid());
    if (write(fd, pid_buf, len) != len) {
        perror("write .monitor_pid");
        close(fd);
        unlink(".monitor_pid");
        return 1;
    }
    close(fd);

    write_message("monitor_reports: started");

    while (keep_running)
        pause();

    if (got_sigint)
        unlink(".monitor_pid");

    return 0;
}
