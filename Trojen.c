/* applet_killer_power.c — "maximum power" C port 
 *
 * WARNING / READ FIRST
 * --------------------

 * The demo therefore runs the "applet" in a CHILD PROCESS forked from
 * a supervisor. The killer runs inside the child, sweeps
 * /proc/self/task, cancels what it can, and finally escalates to
 * SIGKILL on the whole child process — reproducing Java's "stop every
 * thread in the VM" as faithfully as Linux allows.
 *
 * Compile:
 *   cc -std=c11 -Wall -Wextra -pthread -O2 -o applet_killer_power \
 *      applet_killer_power.c
 * Run:
 *   ./main.c 
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* =====================================================================
 * Tunables
 * ===================================================================== */
#define KILLER_NAME       "killer"
#define SWEEP_INTERVAL_MS 100          /* killer.sleep(100) */
#define VICTIM_COUNT      4

/* =====================================================================
 * Helpers
 * ===================================================================== */
static pid_t gettid_(void) { return (pid_t)syscall(SYS_gettid); }

static void msleep(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* =====================================================================
 * /proc/self/task sweep — closest C analogue to Java's ThreadGroup
 * root walk. Enumerates EVERY thread in this process, including ones
 * we never registered (libc, other libs, etc.).
 * ===================================================================== */
typedef struct { pid_t tid; char comm[32]; } TaskInfo;

static int list_tasks(TaskInfo *out, int max) {
    DIR *d = opendir("/proc/self/task");
    if (!d) return 0;
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < max) {
        if (de->d_name[0] == '.') continue;
        char *end;
        long tid = strtol(de->d_name, &end, 10);
        if (*end) continue;
        out[n].tid = (pid_t)tid;
        /* read /proc/self/task/<tid>/comm for the thread name */
        char path[64];
        snprintf(path, sizeof path, "/proc/self/task/%ld/comm", tid);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t r = read(fd, out[n].comm, sizeof(out[n].comm) - 1);
            if (r > 0) {
                out[n].comm[r] = '\0';
                char *nl = strchr(out[n].comm, '\n');
                if (nl) *nl = '\0';
            } else {
                strcpy(out[n].comm, "?");
            }
            close(fd);
        } else {
            strcpy(out[n].comm, "?");
        }
        n++;
    }
    closedir(d);
    return n;
}

/* =====================================================================
 * killOneThread — three escalating strategies, matching the demo's
 * intent: 1) ask politely (pthread_cancel), 2) async-cancel (unsafe but
 * forceful within the process), 3) tgkill with SIGKILL (process-wide).
 *
 * We only reach step 3 on the final pass; before that, the demo shows
 * exactly how far each step gets.
 * ===================================================================== */
typedef struct {
    pid_t tid;
    char  comm[32];
} Victim;

static void request_cancel(pid_t tid) {
    /* pthread_cancel takes a pthread_t, not a tid. We don't have a
     * mapping in the /proc sweep, so this helper can only handle
     * pthread_t we kept. In the power demo, use tgkill(SIGRTMIN) to
     * trigger a cancellation-like signal, or SIGKILL directly. */
    (void)tid;
}

static int force_kill_tid(pid_t tid) {
    /* tgkill: kill exactly this thread with SIGKILL. For SIGKILL the
     * kernel tears down the whole thread group — see WARNING above. */
    pid_t tgid = getpid();
    int rc = syscall(SYS_tgkill, tgid, tid, SIGKILL);
    if (rc != 0)
        fprintf(stderr, "[killer] tgkill(%d,%d,SIGKILL): %s\n",
                (int)tgid, (int)tid, strerror(errno));
    return rc;
}

static int force_kill_all_tasks(int skip_tid) {
    TaskInfo tasks[256];
    int n = list_tasks(tasks, 256);
    int killed = 0;
    for (int i = 0; i < n; i++) {
        if (tasks[i].tid == skip_tid)         continue;  /* don't kill self */
        if (!strcmp(tasks[i].comm, KILLER_NAME)) continue;
        printf("[killer] tgkill SIGKILL -> tid=%d comm='%s'\n",
               (int)tasks[i].tid, tasks[i].comm);
        if (force_kill_tid(tasks[i].tid) == 0) killed++;
    }
    return killed;
}

/* =====================================================================
 * The killer thread.
 *
 * Behavior:
 *   Phase 1 (gentle):  pthread_cancel all pthread_t we know about.
 *   Phase 2 (sweep):   enumerate /proc/self/task, print survivors.
 *   Phase 3 (forced):  tgkill SIGKILL on every tid. Because SIGKILL
 *                      is process-directed, this ends the whole demo
 *                      process — which is the C analogue of "stop all
 *                      threads in the JVM".
 *
 * The phase advance is timed so the demo is observable.
 * ===================================================================== */
typedef struct {
    pthread_t  coop[VICTIM_COUNT];
    int        n_coop;
} KillContext;

static KillContext g_ctx;

static void *killer_run(void *arg) {
    (void)arg;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pid_t self = gettid_();
    printf("[killer] started tid=%d\n", (int)self);

    /* Phase 1: polite pthread_cancel on registered cooperative victims */
    msleep(500);
    for (int i = 0; i < g_ctx.n_coop; i++) {
        int rc = pthread_cancel(g_ctx.coop[i]);
        printf("[killer] phase1 pthread_cancel(coop[%d]) -> %s\n",
               i, rc == 0 ? "requested" : "failed");
    }

    /* Phase 2: sweep /proc/self/task, report survivors */
    msleep(1500);
    TaskInfo tasks[256];
    int n = list_tasks(tasks, 256);
    printf("[killer] phase2 sweep found %d thread(s):\n", n);
    for (int i = 0; i < n; i++)
        printf("         tid=%d comm='%s'\n", (int)tasks[i].tid, tasks[i].comm);

    /* Phase 3: forced kill (process-fatal on Linux). */
    msleep(1500);
    printf("[killer] phase3: escalating to SIGKILL (process-fatal)\n");
    force_kill_all_tasks(self);

    /* In practice we never get here on Linux: SIGKILL to any tid in
     * our thread group terminates all of us. If we somehow survive
     * (e.g. all other threads already dead), keep looping. */
    for (;;) msleep(SWEEP_INTERVAL_MS);
    return NULL;
}

/* =====================================================================
 * Victims
 * ===================================================================== */
static void coop_cleanup(void *arg) {
    printf("[%s] cleanup (cancelled)\n", (const char *)arg);
}
static void *cooperative_victim(void *arg) {
    const char *me = (const char *)arg;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,  NULL);
    pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);
    pthread_cleanup_push(coop_cleanup, (void *)me);
    for (;;) {
        printf("[%s] alive\n", me);
        sleep(1);
    }
    pthread_cleanup_pop(0);
    return NULL;
}

static void *stubborn_victim(void *arg) {
    const char *me = (const char *)arg;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    for (;;) {
        printf("[%s] stubbornly alive\n", me);
        sleep(1);
    }
    return NULL;
}

/* =====================================================================
 * Child process: the "applet VM". Runs all victims + the killer.
 * ===================================================================== */
static int run_applet_vm(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== [child %d] applet VM starting ===\n", (int)getpid());

    memset(&g_ctx, 0, sizeof g_ctx);

    /* Cooperative victims */
    for (int i = 0; i < VICTIM_COUNT / 2; i++) {
        char *name = malloc(32);
        snprintf(name, 32, "coop-%d", i);
        pthread_create(&g_ctx.coop[g_ctx.n_coop++], NULL,
                       cooperative_victim, name);
    }
    /* Stubborn victims */
    for (int i = 0; i < VICTIM_COUNT / 2; i++) {
        char *name = malloc(32);
        snprintf(name, 32, "stub-%d", i);
        pthread_t t;
        pthread_create(&t, NULL, stubborn_victim, name);
        pthread_detach(t);
    }

    /* Killer at max scheduling */
    pthread_t k;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    struct sched_param sp;
    memset(&sp, 0, sizeof sp);
    sp.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (pthread_create(&k, &attr, killer_run, NULL) != 0) {
        printf("[child] SCHED_FIFO denied; using default priority\n");
        pthread_create(&k, NULL, killer_run, NULL);
    }
    pthread_attr_destroy(&attr);

    /* Wait for the killer to nuke the process. If SIGKILL lands, we
     * never return. If SIGKILL is somehow blocked, sleep forever. */
    pthread_join(k, NULL);
    for (;;) pause();
    return 0;
}

/* =====================================================================
 * Supervisor — forked child gets restarted if it dies, mirroring
 * Java's finally { new Thread(...).start(); } resurrection.
 * ===================================================================== */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== AppletKiller 'maximum power' demo (C) ===\n");
    printf("Child will self-SIGKILL after ~3.5s; supervisor respawns it.\n\n");

    const int incarnations = 2;
    for (int i = 0; i < incarnations; i++) {
        printf("[supervisor] spawning applet VM incarnation %d\n", i + 1);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            prctl(PR_SET_PDEATHSIG, SIGKILL);   /* die if parent dies */
            _exit(run_applet_vm());
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status))
            printf("[supervisor] child %d killed by signal %d; respawning\n",
                   (int)pid, WTERMSIG(status));
        else
            printf("[supervisor] child %d exited %d\n",
                   (int)pid, WEXITSTATUS(status));
        msleep(250);
    }
    printf("[supervisor] done after %d incarnations\n", incarnations);
    return 0;
}
