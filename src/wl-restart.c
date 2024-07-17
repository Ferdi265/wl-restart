#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "wl-socket.h"

typedef struct {
    struct wl_socket * socket;

    char * compositor_path;
    char ** compositor_argv;
    int compositor_argc;

    int restart_counter;
    int max_restarts;
} ctx_t;

void init(ctx_t * ctx) {
    ctx->socket = NULL;

    ctx->compositor_path = NULL;
    ctx->compositor_argv = NULL;
    ctx->compositor_argc = 0;

    ctx->restart_counter = 0;
    ctx->max_restarts = 10;
}

void cleanup(ctx_t * ctx) {
    // close socket
    if (ctx->socket != NULL) {
        wl_socket_destroy(ctx->socket);
        ctx->socket = NULL;
    }

    // free path
    if (ctx->compositor_path != NULL) {
        free(ctx->compositor_path);
        ctx->compositor_path = NULL;
    }

    // free argv
    if (ctx->compositor_argv != NULL) {
        for (int i = 0; i < ctx->compositor_argc; i++) {
            free(ctx->compositor_argv[i]);
        }

        free(ctx->compositor_argv);
        ctx->compositor_argv = NULL;
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

    // TODO: resolve compositor path
    ctx->compositor_path = strdup(ctx->compositor_argv[0]);

    // add extra arguments
    char * socket_name = strdup(wl_socket_get_display_name(ctx->socket));
    ctx->compositor_argv[argc + 0] = strdup("--socket");
    ctx->compositor_argv[argc + 1] = socket_name;

    char * socket_fd = NULL;
    asprintf(&socket_fd, "%d", wl_socket_get_fd(ctx->socket));
    ctx->compositor_argv[argc + 2] = strdup("--socket-fd");
    ctx->compositor_argv[argc + 3] = socket_fd;
}

int run_compositor(ctx_t * ctx) {
    pid_t pid = fork();
    if (pid == 0) {
        execv(ctx->compositor_path, ctx->compositor_argv);
        exit(1);
    }

    int stat;
    waitpid(pid, &stat, 0);
    return stat;
}

void run(ctx_t * ctx) {
    while (ctx->restart_counter < ctx->max_restarts) {
        int status = run_compositor(ctx);

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
        run(&ctx);
    }
}
