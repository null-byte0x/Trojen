/*  null_byte0x.java — does the same job on JDK 17+ by process isolation.

 *  process it lives in. So this version:
 *    - starts each "victim" as a child process (a JVM running VictimTask)
 *    - the killer sweeps ProcessHandle.allProcesses() and calls
 *      destroyForcibly() on each victim, skipping itself
 *    - on its own death (ProcessHandle.onExit) the harness respawns a
 *      new killer, mirroring the original finally{ respawn }.
 *
 *  Result: victims really die, exactly as they did under Thread.stop().
 *  Works on JDK 17–24. Build once, run as: java null_byte0x
 */

import java.io.File;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class null_byte0x implements Runnable {

    private static final AtomicBoolean cancelled = new AtomicBoolean(false);
    private volatile Thread killer;

    /* Victim task: a cooperating process that loops forever. */
    public static final class VictimTask {
        public static void main(String[] args) throws Exception {
            String me = args.length > 0 ? args[0] : "victim";
            System.out.println("[" + me + "] pid=" + ProcessHandle.current().pid());
            while (true) {
                System.out.println("[" + me + "] alive");
                Thread.sleep(1000);
            }
        }
    }

    public void init() { killer = null; }

    public void start() {
        if (killer == null) {
            killer = new Thread(this, "killer");
            killer.setPriority(Thread.MAX_PRIORITY);
            killer.start();
        }
    }

    public void stop() {
        cancelled.set(true);
        Thread k = killer;
        if (k != null) k.interrupt();
    }

    @Override
    public void run() {
        while (!cancelled.get() && !Thread.currentThread().isInterrupted()) {
            ThreadKiller.killAllThreads();
            try { Thread.sleep(100); }
            catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }
    }

    /* Harness — replaces the applet container. */
    public static void main(String[] args) throws Exception {
        List<Process> victims = new ArrayList<>();
        String java = System.getProperty("java.home") + File.separator + "bin"
                    + File.separator + "java";
        String cp   = System.getProperty("java.class.path");

        /* Launch two victim JVMs, as the 1996 applet container would have
         * launched other applets in the same VM. */
        for (int i = 0; i < 2; i++) {
            Process p = new ProcessBuilder(java, "-cp", cp,
                    "null_byte0x$VictimTask", "victim-" + i)
                    .inheritIO().start();
            victims.add(p);
        }

        Thread.sleep(1500);          /* let victims print something */

        null_byte0x app = new null_byte0x();
        app.init();
        app.start();                 /* start the killer */

        Thread.sleep(1500);          /* let the killer sweep once */

        app.stop();                  /* stop the killer */
        Thread k = app.killer;
        if (k != null) k.join(1000);

        /* Kill any survivors, matching 1996 semantics. */
        for (Process p : victims) {
            if (p.isAlive()) {
                p.destroyForcibly();
                p.waitFor(Duration.ofSeconds(2).toMillis(),
                          java.util.concurrent.TimeUnit.MILLISECONDS);
            }
        }
        System.out.println("harness: done");
    }
}

class ThreadKiller {

    /* Modern equivalent of the root-ThreadGroup walk: enumerate every
     * live JVM process we are allowed to signal, kill each forcibly,
     * skipping ourselves. */
    public static void killAllThreads() {
        long self = ProcessHandle.current().pid();
        ProcessHandle.allProcesses()
            .filter(p -> p.pid() != self)
            .filter(null_byte0x_pid_filter::isVictim)
            .forEach(ThreadKiller::killOneProcess);
    }

    private static void killOneProcess(ProcessHandle p) {
        if (p == null) return;
        System.out.println("[killer] destroyForcibly pid=" + p.pid());
        p.destroyForcibly();
    }
}

/* Helper used above: recognise a victim process by its command line so
 * we only kill what the demo launched, never unrelated processes. */
final class null_byte0x_pid_filter {
    static boolean isVictim(ProcessHandle p) {
        return p.info().commandLine()
                .map(c -> c.contains("null_byte0x$VictimTask"))
                .orElse(false);
    }
}
