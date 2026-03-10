#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <stdint.h>
#include <errno.h>

#define FORKSRV_FD 198
#define MAP_SIZE 65536

static uint8_t *trace_bitmap = NULL;

/* ========== SHM setup ========== */

static void map_shm(void)
{
    const char *id_str = getenv("__AFL_SHM_ID");
    if (!id_str) {
        fprintf(stderr, "[proxy] __AFL_SHM_ID not set\n");
        return;
    }

    uint32_t shm_id = atoi(id_str);

    /* adresse fixe via AFL_MAP_ADDR si définie */
    void *hint = NULL;
    const char *addr_str = getenv("AFL_MAP_ADDR");
    if (addr_str) hint = (void *)strtoull(addr_str, NULL, 0);

    trace_bitmap = (uint8_t *)shmat(shm_id, hint, 0);
    if (trace_bitmap == (void *)-1) {
        perror("[proxy] shmat");
        exit(1);
    }

    /* Écrire quelque chose pour que LibAFL ne rejette pas le premier input */
    trace_bitmap[0] = 1;
}

/* ========== Hardware tracing placeholders ========== */
/* Remplacer par l'implémentation matérielle (CoreSight, Intel PT, etc.)   */
/* Le hardware doit écrire la couverture dans trace_bitmap (0x6800000000)   */

static void hw_trace_init(pid_t child_pid)
{
    (void)child_pid;
}

static void hw_trace_start(pid_t child_pid)
{
    (void)child_pid;
}

static void hw_trace_stop(void)
{
    /* La couverture hardware devrait maintenant être dans trace_bitmap */
}

/* ========== Forkserver protocol ========== */

int main(int argc, char *argv[])
{
    char **target_argv = NULL;
    int i;

    /* Parse arguments: cherche le séparateur -- */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--") && i + 1 < argc) {
            target_argv = &argv[i + 1];
            break;
        }
    }

    if (!target_argv) {
        fprintf(stderr, "Usage: %s [options] -- target [args...]\n", argv[0]);
        return 1;
    }

    /* Map la bitmap SHM à l'adresse fixe */
    map_shm();

    /* Init hardware tracing */
    hw_trace_init(0);

    /* Dire à LibAFL qu'on est prêt */
    uint32_t status = 0;
    if (write(FORKSRV_FD + 1, &status, 4) != 4) {
        perror("[proxy] handshake write");
        return 1;
    }

    /* Boucle forkserver */
    while (1) {
        int32_t was_killed;

        /* Attendre que LibAFL dise "go" */
        if (read(FORKSRV_FD, &was_killed, 4) != 4) return 0;

        pid_t child_pid = fork();
        if (child_pid < 0) {
            perror("[proxy] fork");
            return 1;
        }

        if (child_pid == 0) {
            /* Enfant : fermer les FDs forkserver, exec le target */
            close(FORKSRV_FD);
            close(FORKSRV_FD + 1);

            execvp(target_argv[0], target_argv);
            perror("[proxy] execvp");
            _exit(1);
        }

        /* Parent : reporter le PID à LibAFL */
        if (write(FORKSRV_FD + 1, &child_pid, 4) != 4) return 1;

        /* Démarrer le tracing hardware */
        hw_trace_start(child_pid);

        /* Attendre la fin du child */
        int child_status;
        waitpid(child_pid, &child_status, 0);

        /* Arrêter le tracing — la bitmap devrait être remplie */
        hw_trace_stop();

        /* Reporter le status à LibAFL */
        if (write(FORKSRV_FD + 1, &child_status, 4) != 4) return 1;
    }

    return 0;
}
