#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define FORKSRV_FD 198

__attribute__((constructor(50)))
void __kill_afl_forkserver(void) {
    dup2(FORKSRV_FD,     300);
    dup2(FORKSRV_FD + 1, 301);
    close(FORKSRV_FD);
    close(FORKSRV_FD + 1);
}

__attribute__((constructor(101)))
void __afl_forkserver_start(void) {

    if (fcntl(300, F_GETFD) == -1) return;

    dup2(300, FORKSRV_FD);
    dup2(301, FORKSRV_FD + 1);
    close(300);
    close(301);

    // ✅ mkdir direct — pas de system() qui fork sh
    mkdir("/tmp/traces", 0777);

    int tmp = 0;
    write(FORKSRV_FD + 1, &tmp, 4);

    while (1) {
        if (read(FORKSRV_FD, &tmp, 4) != 4) return;

        pid_t child = fork();

        if (child == 0) {
            close(FORKSRV_FD);
            close(FORKSRV_FD + 1);

            char *target = getenv("FUZZ_TARGET");
            if (!target) _exit(1);

            // ✅ lit les args du PARENT — pas du child
            const char *path = "/proc/self/cmdline";
            char cmdline[4096] = {0};
            int fd = open(path, O_RDONLY);
            if (fd >= 0) { read(fd, cmdline, sizeof(cmdline)-1); close(fd); }

            // dernier argument = fichier input
            char *input_file = NULL;
            char *p = cmdline;
            int i = 0;
            while (p < cmdline + 4094) {
                if (*p) {
                    input_file = p;
                    p += strlen(p) + 1;
                } else p++;
                if (!*p && !*(p+1)) break;
            }


            if (!input_file) _exit(1);

            char trace_output[256];
            snprintf(trace_output, sizeof(trace_output),
                     "/tmp/traces/trace_%d", getpid());

            pid_t strace_pid = fork();
            if (strace_pid == 0) {
                execl("/usr/bin/strace", "strace",
                      "-e", "trace=read,write",
                    //   "-o", trace_output,
                      "--", target, input_file, NULL);
                _exit(1);
            }

            int st = 0;
            waitpid(strace_pid, &st, 0);

            if (WIFEXITED(st))   _exit(WEXITSTATUS(st));
            if (WIFSIGNALED(st)) kill(getpid(), WTERMSIG(st));
            _exit(0);
        }

        write(FORKSRV_FD + 1, &child, 4);
        int status = 0;
        waitpid(child, &status, 0);
        write(FORKSRV_FD + 1, &status, 4);

    }
}