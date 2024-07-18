#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include "wl-socket.h"

typedef struct {
    struct wl_socket * socket;

    char ** compositor_argv;
    int compositor_argc;

    pid_t compositor_pid;
    int restart_counter;
    int max_restarts;
} ctx_t;

void init(ctx_t * ctx) {
    ctx->socket = NULL;

    ctx->compositor_argv = NULL;
    ctx->compositor_argc = 0;

    ctx->compositor_pid = -1;
    ctx->restart_counter = 0;
    ctx->max_restarts = 10;
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
            free(ctx->compositor_argv[i]);
        }

        free(ctx->compositor_argv);
        ctx->compositor_argv = NULL;
    }

    if (ctx->compositor_pid != -1) {
        kill(ctx->compositor_pid, SIGTERM);
        ctx->compositor_pid = -1;
    }
}

void exit_fail(ctx_t * ctx) {
    cleanup(ctx);
    exit(1);
}

void create_socket(ctx_t * ctx, int argc, char ** argv) {
    ctx->socket = wl_socket_create();
    if (ctx->socket == NULL) {
        printf("error: failed to create wayland socket\n");
        exit_fail(ctx);
    }

    // allocate space for two extra options with arguments + null terminator
    ctx->compositor_argc = argc + 4;
    ctx->compositor_argv = calloc(ctx->compositor_argc + 1, sizeof (char *));

    // copy compositor arguments into argv array
    for (int i = 0; i < argc; i++) {
        ctx->compositor_argv[i] = strdup(argv[i]);
    }

    // add extra arguments
    char * socket_name = strdup(wl_socket_get_display_name(ctx->socket));
    ctx->compositor_argv[argc + 0] = strdup("--socket");
    ctx->compositor_argv[argc + 1] = socket_name;

    char * socket_fd = NULL;
    if (asprintf(&socket_fd, "%d", wl_socket_get_fd(ctx->socket)) == -1) {
        printf("error: failed to convert fd to string\n");
        exit_fail(ctx);
    }

    ctx->compositor_argv[argc + 2] = strdup("--wayland-fd");
    ctx->compositor_argv[argc + 3] = socket_fd;
}

static ctx_t * signal_ctx = NULL;
void handle_term_signal(int signal) {
    ctx_t * ctx = signal_ctx;

    printf("info: signal %s received, exiting\n", strsignal(signal));
    exit_fail(ctx);
}

void handle_restart_signal(int signal) {
    ctx_t * ctx = signal_ctx;

    if (ctx->compositor_pid != -1) {
        printf("info: signal %s received, restarting compositor\n", strsignal(signal));
        kill(ctx->compositor_pid, SIGTERM);
    }
}

void register_signals(ctx_t * ctx) {
    signal_ctx = ctx;
    signal(SIGINT, handle_term_signal);
    signal(SIGTERM, handle_term_signal);
    signal(SIGHUP, handle_restart_signal);
}

void start_compositor(ctx_t * ctx) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(ctx->compositor_argv[0], ctx->compositor_argv);
        exit(1);
    } else {
        ctx->compositor_pid = pid;
    }
}

int wait_compositor(ctx_t * ctx) {
    int stat;
    while (waitpid(ctx->compositor_pid, &stat, 0) == -1) {
        if (errno != EINTR) {
            printf("error: failed to wait for compositor: %s\n", strerror(errno));
            exit_fail(ctx);
        } else {
            // interrupted by signal
            // try again
        }
    }

    ctx->compositor_pid = -1;
    return stat;
}

void run(ctx_t * ctx) {
    while (ctx->restart_counter < ctx->max_restarts) {
        start_compositor(ctx);
        int status = wait_compositor(ctx);

        if (!WIFSIGNALED(status) && WEXITSTATUS(status) == 0) {
            printf("info: compositor exited successfully, quitting\n");
            cleanup(ctx);
            exit(0);
        } else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTRAP) {
            ctx->restart_counter = 0;
            printf("info: compositor exited with SIGTRAP, resetting counter\n");
        } else {
            ctx->restart_counter++;
            printf("info: compositor exited with code %d, incrementing restart counter (%d)\n", status, ctx->restart_counter);
        }
    }

    printf("error: too many restarts, quitting\n");
    exit_fail(ctx);
}

void usage(ctx_t * ctx) {
    printf("usage: wl-restart [[options] --] <compositor args>\n");
    printf("\n");
    printf("compositor restart helper. restarts your compositor when it\n");
    printf("crashes and keeps the wayland socket alive.\n");
    printf("\n");
    printf("options:\n");
    printf("  -h,   --help             show this help\n");
    printf("  -n N, --max-restarts N   restart a maximum of N times (default 10)\n");
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
                printf("error: option '%s' needs an argument\n", opt);
                exit_fail(&ctx);
            }

            argc--, argv++;
            ctx.max_restarts = atoi(arg);
        } else if (strcmp(opt, "--") == 0) {
            break;
        } else {
            printf("error: unknown option '%s', see --help\n", opt);
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
