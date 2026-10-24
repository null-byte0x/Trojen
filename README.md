<div align="center">

<img src="https://capsule-render.vercel.app/api?type=venom&color=0:050505,40:003d0f,75:00a82b,100:00ff41&height=240&section=header&text=null-byte0x&fontSize=68&fontColor=ffffff&animation=twinkling&fontAlignY=38&desc=Java%20%7C%20C11%20%7C%20Security%20Research%20Tool&descSize=13&descAlignY=62" width="100%"/>

[![Typing SVG](https://readme-typing-svg.demolab.com?font=Fira+Code&weight=700&size=17&duration=2000&pause=700&color=39ff6e&center=true&vCenter=true&width=820&lines=%24+javac+null_byte0x.java+%26%26+java+null_byte0x;%24+cc+-std%3Dc11+-pthread+-O2+-o+null_byte0x+null_byte0x.c;%5B%2B%5D+compiled+successfully;%5B%2B%5D+running...;%5B%E2%9C%93%5D+done)](https://git.io/typing-svg)

![Java](https://img.shields.io/badge/Java-JDK-ED8B00?style=for-the-badge&logo=openjdk&logoColor=white)
![C](https://img.shields.io/badge/C-C11-00599C?style=for-the-badge&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Threading](https://img.shields.io/badge/Threading-pthreads-39ff6e?style=for-the-badge&logoColor=black)
![Author](https://img.shields.io/badge/by-null--byte0x-111111?style=for-the-badge&logo=github&logoColor=39ff6e)

<br/>

**✦ ━━━━━━━ dual-language · research · systems ━━━━━━━ ✦**

</div>

---

## `$ cat overview.md`

**null_byte0x** is a dual-language security research tool implemented in both **Java** and **C11**. Both implementations share the same core logic, letting you compare how the same low-level behaviour is expressed across a managed runtime (JVM) and bare-metal systems code.

```text
 implementations
 ├── ☕  null_byte0x.java   — JVM version (javac / java)
 └── ⚙️  null_byte0x.c     — C11 version (cc + pthreads)
```

> Add your project description here — what does it do, what does it research or demonstrate?

---

## `$ cat build_and_run.md`

### ☕ Java — JDK

#### Requirements
- JDK 8 or newer (`javac` + `java` on PATH)

#### Compile
```bash
javac null_byte0x.java
```

#### Run
```bash
java null_byte0x
```

#### One-liner
```bash
javac null_byte0x.java && java null_byte0x
```

---

### ⚙️ C — C11 + pthreads

#### Requirements
- Linux
- GCC or Clang with C11 support
- pthreads (`-pthread`)

#### Compile
```bash
cc -std=c11 -Wall -Wextra -pthread -O2 \
   -o null_byte0x null_byte0x.c
```

#### Run
```bash
./null_byte0x
```

#### One-liner
```bash
cc -std=c11 -Wall -Wextra -pthread -O2 -o null_byte0x null_byte0x.c && ./null_byte0x
```

---

### Build Flags Explained

| Flag | Language | Purpose |
|---|---|---|
| `javac` | Java | Compiles `.java` → `.class` bytecode |
| `java` | Java | Runs bytecode on the JVM |
| `-std=c11` | C | Enforces C11 standard |
| `-Wall -Wextra` | C | Full compiler warnings |
| `-pthread` | C | Links libpthread, enables POSIX threads |
| `-O2` | C | Optimised build — realistic runtime behaviour |

---

## `$ diff java_vs_c.md`

| | Java | C |
|---|---|---|
| **Runtime** | JVM (managed) | Native (bare metal) |
| **Memory** | GC-managed heap | Manual (`malloc`/`free`) |
| **Threads** | `java.lang.Thread` | `pthread_t` |
| **Compile** | `javac null_byte0x.java` | `cc ... null_byte0x.c` |
| **Run** | `java null_byte0x` | `./null_byte0x` |
| **Portability** | Any JDK platform | Linux (uses `/proc`) |
| **Build output** | `null_byte0x.class` | `null_byte0x` binary |

---

## `$ ls repo/`

```
null_byte0x/
├── README.md
├── null_byte0x.java      ← Java implementation
└── null_byte0x.c         ← C11 implementation
```

---

## `$ cat disclaimer.txt`

> [!NOTE]
> **This is a security research tool.**

```
All code in this repository is for educational and research
purposes only. Run only on systems you own or have explicit
written permission to test.

The author (null-byte0x) holds no liability for misuse.
```

---

<div align="center">

[![GitHub](https://img.shields.io/badge/GitHub-null--byte0x-111111?style=for-the-badge&logo=github&logoColor=39ff6e)](https://github.com/null-byte0x)

[![Typing SVG](https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=14&duration=2500&pause=900&color=39ff6e&center=true&vCenter=true&width=600&lines=0x00+javac+null_byte0x.java;0x01+java+null_byte0x;0x02+cc+-pthread+-O2+null_byte0x.c;0x03+.%2Fnull_byte0x;End+of+transmission._)](https://git.io/typing-svg)

<img src="https://capsule-render.vercel.app/api?type=waving&color=0:00ff41,50:005c1a,100:050505&height=130&section=footer" width="100%"/>

</div>
