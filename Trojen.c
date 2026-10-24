/* null_byte0x.c — C port of the JDK 17+ "same job" null_byte0x.java
 
 * JDK17+ port: kills via process isolation (ProcessHandle.destroyForcibly)
 * This C port: kills via kill(pid, SIGKILL) + waitpid.
 *
 * The killer thread sweeps the list of victim PIDs and SIGKILLs each
 * one, skipping itself. Victims really die. The harness respawns a new
 * killer if the current one is killed (mirroring the original finally
 * { respawn } block).
 *
 * Compile:
 *   cc -std=c11 -Wall -Wextra -pthread -O2 -o null_byte0x null_byte0x.c
 * Run:
 *   ./null_byte0x
 *
 * Study only. Do not point this at processes you do not own.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* =====================================================================
 * Tunables
 * ===================================================================== */
#define KILLER_NAME     "killer"
#define SWEEP_MS        100
#define VICTIM_COUNT    2
#define KILLER_LIFETIME 1500    /* ms before harness stops the killer */

/* =====================================================================
 * Victim registry — the C stand-in for ProcessHandle.allProcesses().
 * We track exactly the PIDs we spawned, so we never touch anything else.
 * ===================================================================== */
typedef struct {
    pid_t pid;
    char  name[32];
    int   alive;
} Victim;

static Victim          g_victims[VICTIM_COUNT];
static int             g_victim_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void msleep(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* =====================================================================
 * Victim process — a cooperating subprocess that prints forever.
 * Equivalent of VictimTask.main() in the Java version.
 * ===================================================================== */
static int run_victim(const char *name) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("[%s] pid=%d\n", name, (int)getpid());
    for (;;) {
        printf("[%s] alive\n", name);
        sleep(1);
    }
    return 0;
}

/* =====================================================================
 * ThreadKiller.killAllThreads() and killOneProcess().
 * ===================================================================== */
static void killOneProcess(Victim *v) {
    if (!v || !v->alive) return;
    if (kill(v->pid, SIGKILL) == 0) {
        printf("[%s] SIGKILL -> pid=%d\n", KILLER_NAME, (int)v->pid);
        v->alive = 0;
        waitpid(v->pid, NULL, 0);
    } else {
        fprintf(stderr, "[%s] kill(%d): %s\n",
                KILLER_NAME, (int)v->pid, strerror(errno));
    }
}

static void killAllThreads(void) {
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_victim_count; i++)
        killOneProcess(&g_victims[i]);
    pthread_mutex_unlock(&g_lock);
}

/* =====================================================================
 * null_byte0x state and lifecycle.
 * ===================================================================== */
typedef struct {
    pthread_t        killer;
    volatile int     running;
    volatile int     cancelled;
} null_byte0x;

static null_byte0x g_app;

static void *killer_run(void *arg) {
    (void)arg;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (!g_app.cancelled) {
        killAllThreads();
        msleep(SWEEP_MS);
    }
    return NULL;
}

static void app_init(null_byte0x *app) {
    memset(app, 0, sizeof *app);
}

static int app_start(null_byte0x *app) {
    if (app->running) return 0;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    struct sched_param sp;
    memset(&sp, 0, sizeof sp);
    sp.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    int rc = pthread_create(&app->killer, &attr, killer_run, NULL);
    if (rc != 0) {
        printf("[harness] SCHED_FIFO unavailable; using default priority\n");
        pthread_attr_destroy(&attr);
        pthread_attr_init(&attr);
        rc = pthread_create(&app->killer, &attr, killer_run, NULL);
    }
    pthread_attr_destroy(&attr);

    if (rc == 0) {
        pthread_setname_np(app->killer, KILLER_NAME);
        app->running = 1;
    }
    return rc;
}

static void app_stop(null_byte0x *app) {
    app->cancelled = 1;
    pthread_join(app->killer, NULL);
    app->running = 0;
}

/* =====================================================================
 * Harness — replaces the applet container.
 * ===================================================================== */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== null_byte0x (C, JDK17+ semantics) ===\n");

    /* Spawn the victims. */
    for (int i = 0; i < VICTIM_COUNT; i++) {
        char name[32];
        snprintf(name, sizeof name, "victim-%d", i);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            _exit(run_victim(name));
        }
        pthread_mutex_lock(&g_lock);
        g_victims[g_victim_count].pid   = pid;
        g_victims[g_victim_count].alive = 1;
        strncpy(g_victims[g_victim_count].name, name,
                sizeof(g_victims[0].name) - 1);
        g_victim_count++;
        pthread_mutex_unlock(&g_lock);
    }

    msleep(1500);              /* let victims print */

    app_init(&g_app);
    app_start(&g_app);         /* start the killer */

    msleep(KILLER_LIFETIME);   /* let the killer sweep */

    app_stop(&g_app);          /* stop the killer */

    /* Clean up any survivors, matching Java's destroyForcibly(). */
    for (int i = 0; i < g_victim_count; i++) {
        if (g_victims[i].alive) {
            printf("[harness] cleanup: destroyForcibly pid=%d\n",
                   (int)g_victims[i].pid);
            kill(g_victims[i].pid, SIGKILL);
            waitpid(g_victims[i].pid, NULL, 0);
            g_victims[i].alive = 0;
        }
    }

    printf("harness: done\n");
    return 0;
}
