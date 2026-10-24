<div align="center">

<img src="https://capsule-render.vercel.app/api?type=venom&color=0:050505,40:0a0a2e,75:1a1a6e,100:4444ff&height=240&section=header&text=applet_killer_power&fontSize=48&fontColor=ffffff&animation=twinkling&fontAlignY=38&desc=Linux%20Thread%20Cancellation%20%7C%20C11%20%7C%20Systems%20Research&descSize=13&descAlignY=62" width="100%"/>

[![Typing SVG](https://readme-typing-svg.demolab.com?font=Fira+Code&weight=700&size=17&duration=2000&pause=700&color=4499ff&center=true&vCenter=true&width=820&lines=%24+cc+-std%3Dc11+-Wall+-pthread+-O2+-o+applet_killer_power;%5B%2B%5D+sweeping+%2Fproc%2Fself%2Ftask...;%5B%2B%5D+cancelling+threads...;%5B!%5D+escalating+to+SIGKILL;%5B%E2%9C%93%5D+all+threads+stopped)](https://git.io/typing-svg)

![Language](https://img.shields.io/badge/Language-C11-00599C?style=for-the-badge&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Threading](https://img.shields.io/badge/Threading-pthreads-4444ff?style=for-the-badge&logoColor=white)
![Type](https://img.shields.io/badge/Type-Systems_Research-111111?style=for-the-badge)

<br/>

**✦ ━━━━━━━ fork · sweep · cancel · kill ━━━━━━━ ✦**

</div>

---

## `$ cat overview.md`

**applet_killer_power** is a C11 research demo that faithfully reproduces Java's *"stop every thread in the VM"* behaviour on Linux — the mechanism historically used by the deprecated `Thread.stop()` and the JVM shutdown sequence.

It answers a concrete systems question:

> *How closely can a Linux process replicate the JVM's ability to forcefully terminate every thread, including threads it did not create?*

The answer is: by combining **POSIX thread cancellation** (`pthread_cancel`) swept across `/proc/self/task` with a final **SIGKILL escalation** on the entire child process group.

```text
 what it demonstrates
 ├── 🔀  fork-based supervisor/worker isolation
 ├── 📂  /proc/self/task enumeration (live thread discovery)
 ├── 🧵  pthread_cancel — cooperative cancellation via cancellation points
 ├── ⚡  SIGKILL escalation — non-cooperative hard termination
 └── 🔬  comparison with Java's deprecated Thread.stop() semantics
```

---

## `$ cat how_it_works.md`

### Architecture

The program uses a **two-process model** to safely run a destructive operation:

```
main (supervisor)
│
└─── fork()
     │
     └─── child process  ("the applet")
          ├── spawns N worker threads
          ├── applet_killer runs inside this child
          │    ├── opens /proc/self/task
          │    ├── sends pthread_cancel to each TID
          │    └── escalates to SIGKILL on child PID
          └── child process exits / is killed
               ↑
main waits via waitpid(), reports exit status
```

The supervisor never touches the killer logic — if something goes wrong inside the child, the host process stays clean.

### Thread Sweep via `/proc/self/task`

```c
// enumerate every live thread in the current process
DIR *d = opendir("/proc/self/task");
while ((ent = readdir(d)) != NULL) {
    pid_t tid = atoi(ent->d_name);
    // send cancellation request
    pthread_cancel(tid);   // cooperative — honours cancel points
}
// if threads don't respond: escalate
kill(getpid(), SIGKILL);   // unconditional
```

This mirrors what the JVM does at the native layer when all else fails.

### Why a Child Process?

`SIGKILL` cannot be caught, blocked, or ignored. Sending it to the current process would end the whole program with no cleanup. Running the "applet" in a child lets the supervisor:
- survive the kill
- collect the exit status with `waitpid()`
- report results or restart cleanly

---

## `$ cat build_and_run.md`

### Requirements

- Linux (kernel ≥ 3.0 — `/proc/self/task` required)
- GCC or Clang with C11 support
- pthreads (`-pthread`)

### Compile

```bash
cc -std=c11 -Wall -Wextra -pthread -O2 \
   -o applet_killer_power applet_killer_power.c
```

| Flag | Purpose |
|---|---|
| `-std=c11` | C11 standard (uses `_Noreturn`, `stdatomic`, etc.) |
| `-pthread` | Links libpthread, enables POSIX thread support |
| `-Wall -Wextra` | Full warnings — keep the build clean |
| `-O2` | Optimised binary — realistic timing behaviour |

### Run

```bash
./applet_killer_power
```

Expected output:

```
[supervisor] forked child PID 12345
[child]  spawning worker threads...
[killer] sweeping /proc/self/task
[killer] cancelling TID 12346
[killer] cancelling TID 12347
[killer] cancelling TID 12348
[killer] escalating to SIGKILL
[supervisor] child exited — status: killed by signal 9
```

---

## `$ cat concepts.md`

### pthread_cancel vs SIGKILL

| | `pthread_cancel` | `SIGKILL` |
|---|---|---|
| **Cooperative?** | Yes — thread must reach a cancellation point | No — immediate |
| **Catchable?** | Partially — cleanup handlers via `pthread_cleanup_push` | Never |
| **Scope** | Single thread (by TID) | Entire process (all threads) |
| **Java analogy** | `Thread.interrupt()` + `Thread.stop()` (deprecated) | JVM SIGKILL fallback |
| **Safe for data?** | Mostly — if cancellation points are placed carefully | No — no cleanup |

### Java `Thread.stop()` — why it was deprecated

`Thread.stop()` was deprecated in Java 1.2 because it released all monitors held by the stopped thread, potentially leaving shared objects in an inconsistent state. This demo shows the C equivalent and why the same risks apply at the OS level.

### `/proc/self/task` — live thread enumeration

On Linux, every thread in a process has its own entry under `/proc/<pid>/task/<tid>`. Reading this directory at runtime gives a snapshot of all currently live threads — including threads spawned by libraries the caller did not write. This is the only portable way on Linux to find all threads without cooperation.

---

## `$ cat research_notes.md`

```
┌──────────────────────────────────────────────────────────────┐
│  Key findings from this demo                                 │
│                                                              │
│  1. pthread_cancel is insufficient alone                     │
│      Threads blocked in non-cancellable syscalls             │
│      (e.g. futex, read on a blocking fd) may never           │
│      reach a cancellation point.                             │
│                                                              │
│  2. /proc/self/task has TOCTOU gaps                          │
│      New threads can be spawned between readdir()            │
│      calls. A sweep loop must account for this.              │
│                                                              │
│  3. SIGKILL is the only guaranteed termination               │
│      On Linux, no thread can survive SIGKILL on              │
│      its process. It is the true equivalent of               │
│      the JVM's last-resort shutdown hook.                    │
│                                                              │
│  4. Child process isolation is essential                     │
│      Running the killer in a child makes the demo            │
│      safe to execute on a dev machine without                │
│      risking the terminal session.                           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## `$ cat further_reading.md`

| Resource | Topic |
|---|---|
| `man 7 pthreads` | POSIX thread cancellation semantics |
| `man 5 proc` | `/proc/self/task` and thread entries |
| `man 2 kill` | Signal delivery to processes and threads |
| `man 2 tgkill` | Thread-directed signal delivery by TID |
| JEP 371 — Hidden Classes | Context for JVM thread lifecycle |
| *The Linux Programming Interface* — Kerrisk, Ch. 33 | Thread cancellation deep dive |

---

## `$ cat disclaimer.txt`

> [!NOTE]
> **This is a systems research demo.**

This tool exists to study and document how Linux thread cancellation and process termination work at a low level, in comparison to JVM internals. It:

- runs its destructive logic **inside a child process** — it does not affect the host system
- does **not** interact with any external process or PID
- is intended for **compilation and execution in a development VM or container**

```
Do not point this at production processes or PIDs you do not own.
The author holds no liability for misuse.
```

---

<div align="center">

[![null-byte0x](https://img.shields.io/badge/by-null--byte0x-111111?style=for-the-badge&logo=github&logoColor=4499ff)](https://github.com/null-byte0x)
[![C11](https://img.shields.io/badge/C11-Systems_Research-00599C?style=for-the-badge&logo=c&logoColor=white)]()
[![Linux](https://img.shields.io/badge/Linux_Only-/proc/self/task-FCC624?style=for-the-badge&logo=linux&logoColor=black)]()

[![Typing SVG](https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=14&duration=2500&pause=900&color=4499ff&center=true&vCenter=true&width=600&lines=0x00+fork();0x01+sweep+%2Fproc%2Fself%2Ftask;0x02+pthread_cancel();0x03+SIGKILL;End+of+transmission._)](https://git.io/typing-svg)

<img src="https://capsule-render.vercel.app/api?type=waving&color=0:4444ff,50:0a0a5e,100:050505&height=130&section=footer" width="100%"/>

</div>
