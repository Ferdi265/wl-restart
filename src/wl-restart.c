#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include "wl-socket.h"

// taken from libsystemd <systemd/sd-daemon.h>
// part of the systemd socket activation protocol
#define SD_LISTEN_FDS_START 3

typedef enum {
    MODE_KDE = 0,
    MODE_CLI = 1,
    MODE_ENV = 2,
    MODE_SYSTEMD = 3
} pass_mode_t;

typedef struct {
    struct wl_socket * socket;

    char ** compositor_argv;
    int compositor_argc;

    volatile pid_t compositor_pid;
    int restart_counter;
    int max_restarts;
    pass_mode_t mode;
} ctx_t;

void init(ctx_t * ctx) {
    ctx->socket = NULL;

    ctx->compositor_argv = NULL;
    ctx->compositor_argc = 0;

    ctx->compositor_pid = -1;
    ctx->restart_counter = 0;
    ctx->max_restarts = 10;
    ctx->mode = MODE_KDE;
}

void cleanup(ctx_t * ctx) {
    // close socket
    if (ctx->socket != NULL) {
        wl_socket_destroy(ctx->socket);
        ctx->socket = NULL;
    }

    // free argv
    if (ctx->compositor_argv != NULL) {
        for (int i = 0; i < ctx->compositor_argc; i++) {
            if (ctx->compositor_argv[i] != NULL) free(ctx->compositor_argv[i]);
        }

        free(ctx->compositor_argv);
        ctx->compositor_argv = NULL;
    }

    pid_t compositor_pid = ctx->compositor_pid;
    if (compositor_pid != -1) {
        ctx->compositor_pid = -1;
        kill(compositor_pid, SIGTERM);
    }
}

void exit_fail(ctx_t * ctx) {
    cleanup(ctx);
    exit(1);
}

void create_socket(ctx_t * ctx, int argc, char ** argv) {
    ctx->socket = wl_socket_create();
    if (ctx->socket == NULL) {
        fprintf(stderr, "error: failed to create wayland socket\n");
        exit_fail(ctx);
    }

    // allocate space for two extra options with arguments for cli modes
    ctx->compositor_argc = argc + 4;
    // add 1 for null terminator
    ctx->compositor_argv = calloc(ctx->compositor_argc + 1, sizeof (char *));

    // copy compositor arguments into argv array
    for (int i = 0; i < argc; i++) {
        ctx->compositor_argv[i] = strdup(argv[i]);
    }

    // prepare arguments for cli modes
    if (ctx->mode == MODE_KDE || ctx->mode == MODE_CLI) {
        const char * socket_name_arg = ctx->mode == MODE_KDE ? "--socket" : "--wayland-socket";
        const char * socket_fd_arg = "--wayland-fd";

        // add extra arguments
        char * socket_name = strdup(wl_socket_get_display_name(ctx->socket));
        ctx->compositor_argv[argc + 0] = strdup(socket_name_arg);
        ctx->compositor_argv[argc + 1] = socket_name;

        char * socket_fd = NULL;
        if (asprintf(&socket_fd, "%d", wl_socket_get_fd(ctx->socket)) == -1) {
            fprintf(stderr, "error: failed to convert fd to string\n");
            exit_fail(ctx);
        }

        ctx->compositor_argv[argc + 2] = strdup(socket_fd_arg);
        ctx->compositor_argv[argc + 3] = socket_fd;
    }

    // prepare environment for env modes
    if (ctx->mode == MODE_ENV || ctx->mode == MODE_SYSTEMD) {
        const char * socket_name_var = ctx->mode == MODE_ENV ? "WAYLAND_SOCKET_NAME" : "LISTEN_FDNAMES";
        const char * socket_fd_var = ctx->mode == MODE_ENV ? "WAYLAND_SOCKET_FD" : "LISTEN_FDS";

        // set environment vars
        setenv(socket_name_var, wl_socket_get_display_name(ctx->socket), true);

        if (ctx->mode == MODE_ENV) {
            char * socket_fd = NULL;
            if (asprintf(&socket_fd, "%d", wl_socket_get_fd(ctx->socket)) == -1) {
                fprintf(stderr, "error: failed to convert fd to string\n");
                exit_fail(ctx);
            }

            setenv(socket_fd_var, socket_fd, true);
            free(socket_fd);
        } else if (ctx->mode == MODE_SYSTEMD) {
            // systemd socket activation has a hardcoded fd number
            // and only passes the number of fds
            setenv(socket_fd_var, "1", true);
        }
    }
}

void start_compositor(ctx_t * ctx) {
    pid_t pid = fork();
    if (pid == 0) {
        if (ctx->mode == MODE_SYSTEMD) {
            // set up systemd socket activation

            // set LISTEN_PID env var
            char * listen_pid = NULL;
            if (asprintf(&listen_pid, "%d", getpid()) == -1) {
                fprintf(stderr, "error: failed to convert compositor pid to string\n");
                exit(1);
            }
            setenv("LISTEN_PID", listen_pid, true);
            free(listen_pid);

            // move socket fd to SD_LISTEN_FDS_START
            int socket_fd = wl_socket_get_fd(ctx->socket);
            if (socket_fd != SD_LISTEN_FDS_START) {
                if (dup2(socket_fd, SD_LISTEN_FDS_START) == -1) {
                    fprintf(stderr, "error: failed to move socket fd to SD_LISTEN_FDS_START\n");
                    exit(1);
                }

                // close original fd so it doesn't leak
                close(socket_fd);
            }
        }

        // exec into compositor process
        execvp(ctx->compositor_argv[0], ctx->compositor_argv);

        fprintf(stderr, "error: failed to start compositor\n");
        exit(1);
    } else {
        ctx->compositor_pid = pid;
    }
}

void wait_compositor(ctx_t * ctx, int * exit_status, int * exit_signal) {
    int stat;
    pid_t compositor_pid = ctx->compositor_pid;
    while (compositor_pid != -1 && waitpid(compositor_pid, &stat, 0) == -1) {
        if (errno == EINTR) {
            // interrupted by signal
            // try again
        } else if (errno == ECHILD) {
            // signal handler probably already waited for this child
            // try again
        } else {
            fprintf(stderr, "error: failed to wait for compositor: %s\n", strerror(errno));
            exit_fail(ctx);
        }

        // re-check compositor pid
        // signal handler might have already killed compositor
        compositor_pid = ctx->compositor_pid;
    }

    if (ctx->compositor_pid == -1) {
        // signal handler already killed the compositor
        // simulate SIGHUP
        *exit_status = -1;
        *exit_signal = SIGHUP;
    } else if (WIFSIGNALED(stat)) {
        // compositor killed by signal
        *exit_status = -1;
        *exit_signal = WTERMSIG(stat);
    } else {
        // compositor died normally
        *exit_status = WEXITSTATUS(stat);
        *exit_signal = -1;
    }

    // clear compositor pid
    ctx->compositor_pid = -1;
}

static ctx_t * signal_ctx = NULL;
void handle_quit_signal(int signal) {
    ctx_t * ctx = signal_ctx;

    fprintf(stderr, "info: signal %s received, quitting\n", strsignal(signal));
    exit_fail(ctx);
}

void handle_restart_signal(int signal) {
    ctx_t * ctx = signal_ctx;

    fprintf(stderr, "info: signal %s received, restarting compositor\n", strsignal(signal));
    pid_t compositor_pid = ctx->compositor_pid;
    if (compositor_pid != -1) {
        ctx->compositor_pid = -1;
        kill(compositor_pid, SIGTERM);
    }
}

void register_signals(ctx_t * ctx) {
    signal_ctx = ctx;
    signal(SIGINT, handle_quit_signal);
    signal(SIGTERM, handle_quit_signal);
    signal(SIGHUP, handle_restart_signal);
}

void run(ctx_t * ctx) {
    while (ctx->restart_counter < ctx->max_restarts) {
        start_compositor(ctx);
        int exit_status = -1;
        int exit_signal = -1;
        wait_compositor(ctx, &exit_status, &exit_signal);

        if (exit_status == 0) {
            fprintf(stderr, "info: compositor exited successfully, quitting\n");
            cleanup(ctx);
            exit(0);
        } else if (exit_status > 0) {
            ctx->restart_counter++;
            fprintf(stderr, "info: compositor exited with code %d, incrementing restart counter (%d)\n", exit_status, ctx->restart_counter);
        } else if (exit_signal == SIGHUP) {
            ctx->restart_counter = 0;
            fprintf(stderr, "info: compositor died with signal %s, restarting\n", strsignal(SIGHUP));
        } else if (exit_signal == SIGTRAP) {
            ctx->restart_counter = 0;
            fprintf(stderr, "info: compositor died with signal %s, resetting counter\n", strsignal(SIGTRAP));
        } else if (exit_signal > 0) {
            ctx->restart_counter++;
            fprintf(stderr, "info: compositor died with signal %s, incrementing restart counter (%d)\n", strsignal(exit_signal), ctx->restart_counter);
        } else {
            fprintf(stderr, "error: failed to detect how compositor exited\n");
            exit_fail(ctx);
        }

        // format new restart count
        char * restart_counter_str = NULL;
        if (asprintf(&restart_counter_str, "%d", ctx->restart_counter) == -1) {
            fprintf(stderr, "error: failed to convert restart counter to string\n");
            exit_fail(ctx);
        }

        // add restart count to environment
        setenv("WL_RESTART_COUNT", restart_counter_str, true);
        free(restart_counter_str);
    }

    fprintf(stderr, "error: too many restarts, quitting\n");
    exit_fail(ctx);
}

void usage(ctx_t * ctx) {
    printf("usage: wl-restart [[options] --] <compositor args>\n");
    printf("\n");
    printf("compositor restart helper. restarts your compositor when it\n");
    printf("crashes and keeps the wayland socket alive.\n");
    printf("\n");
    printf("options:\n");
    printf("  -h,   --help           show this help\n");
    printf("  -n N, --max-restarts N restart a maximum of N times (default 10)\n");
    printf("        --kde            pass socket via cli options --socket and --wayland-fd (default)\n");
    printf("        --cli            pass socket via cli options --wayland-socket and --wayland-fd\n");
    printf("        --env            pass socket via env vars WAYLAND_SOCKET_NAME and WAYLAND_SOCKET_FD\n");
    printf("        --systemd        pass socket via env vars LISTEN_PID, LISTEN_FDS, and LISTEN_FDNAMES\n");
    cleanup(ctx);
    exit(0);
}

int main(int argc, char ** argv) {
    if (argc > 0) {
        // skip program name
        argc--, argv++;
    }

    ctx_t ctx;
    init(&ctx);

    while (argv[0] != NULL && argv[0][0] == '-') {
        char * opt = argv[0];
        char * arg = argv[1];
        argc--, argv++;

        if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
            usage(&ctx);
        } else if (strcmp(opt, "-n") == 0 || strcmp(opt, "--max-restarts") == 0) {
            if (arg == NULL) {
                fprintf(stderr, "error: option '%s' needs an argument\n", opt);
                exit_fail(&ctx);
            }

            argc--, argv++;
            ctx.max_restarts = atoi(arg);
        } else if (strcmp(opt, "--kde") == 0) {
            ctx.mode = MODE_KDE;
        } else if (strcmp(opt, "--cli") == 0) {
            ctx.mode = MODE_CLI;
        } else if (strcmp(opt, "--env") == 0) {
            ctx.mode = MODE_ENV;
        } else if (strcmp(opt, "--systemd") == 0) {
            ctx.mode = MODE_SYSTEMD;
        } else if (strcmp(opt, "--") == 0) {
            break;
        } else {
            fprintf(stderr, "error: unknown option '%s', see --help\n", opt);
            exit_fail(&ctx);
        }
    }

    if (argc == 0) {
        usage(&ctx);
    } else {
        create_socket(&ctx, argc, argv);
        register_signals(&ctx);
        run(&ctx);
    }

    cleanup(&ctx);
}
