# Project Requirements Document (PRD)

**Project:** Smart Energy Smart-Meter Pulse Counter & Analytics Agent
**Author:** Abhishek Mohapatra
**Course:** Linux Device Drivers, System Programming and C++ (individual project)
**Repository:** https://github.com/Abhishek993879/smart-meter

---

## 1. Introduction (Stage 1)

### 1.1 Problem statement

Electricity meters report consumption as pulses: a fixed number of pulses means one kilowatt-hour (kWh). A bare pulse stream is not useful to people. Households and small businesses cannot see how much power they are drawing right now, how much energy they have used, what it costs, or when an unusually high load starts. Manual meter reading is slow, and the information arrives too late to change behaviour.

Turning pulses into reliable, timely information needs three things working together:

1. A **kernel-level driver** that captures every pulse with an accurate timestamp, without losing any, even when the application is busy.
2. A **userspace service** that converts pulses into power and energy figures, keeps history, and detects abnormal load.
3. An **interface** where a person can see the results and react.

### 1.2 Objective

Design, build, test, and document a complete software system, from a Linux kernel module to a web dashboard, that counts smart-meter pulses and produces live and historical energy analytics. The project must demonstrate a professional development process: requirements, design, implementation, testing, and final delivery.

### 1.3 Scope

**In scope**

- A Linux kernel module (`pulsecnt`) that simulates a meter's pulse output with a high-resolution timer, timestamps every pulse, buffers pulses, and exposes them through a character device (`/dev/pulsecnt`) supporting `read`, `poll`, and `ioctl`.
- A C++17 daemon (`energy-agent`) using POSIX system programming: file descriptors, `poll()`, threads, signals, and a configuration file.
- Analytics: power in watts, cumulative energy in kWh, cost, peak power, and threshold alerts.
- Persistent storage of readings and alerts in SQLite.
- A built-in HTTP server with a JSON API and a web dashboard (HTML, JavaScript, Chart.js) with live values, a history chart, alerts, and load control.
- A command-line test tool for the driver (`pulsecnt_ctl`).
- A software-only simulator pulse source, so the agent can run without the kernel module.
- Unit, integration, and system tests, plus full documentation and Git history.

**Out of scope**

- Real meter hardware, GPIO wiring, or a real interrupt line. The hrtimer replaces the hardware pulse output. The driver is designed so a GPIO interrupt handler could replace the timer later.
- Cloud services, multi-user accounts, and authentication.
- Billing-grade accuracy, tariff schedules with time-of-use rates, and mobile apps.
- Support for kernels or distributions other than recent Ubuntu LTS releases.

### 1.4 Expected outcome and applications

**Outcome:** a working system that a user can start with two commands (load the driver, start the agent), then open a browser to see live power, energy, cost, and alerts, with data kept across restarts.

**Applications:** home energy monitoring, small-business demand monitoring, detection of unusual loads, teaching material for Linux driver and systems programming, and a base for a real hardware meter reader.

### 1.5 Users

| User | Need |
|---|---|
| Household or business owner | See current power, daily energy and cost, and be told about high load |
| Developer / student | A clear, tested example of a driver, a daemon, and a web interface working together |
| Course assessor | Evidence of requirements, design, implementation, testing, and process |

---

## 2. Functional requirements

Priority: **M** = must have, **S** = should have. Status as of the last update is in section 7.

### Kernel driver

| ID | Requirement | Priority |
|---|---|---|
| FR-01 | The driver shall generate simulated meter pulses at a rate set by a load in watts and a meter constant in pulses per kWh, using a high-resolution timer | M |
| FR-02 | The driver shall timestamp each pulse with wall-clock time and number it with a running count | M |
| FR-03 | The driver shall store pulses in a fixed-size ring buffer protected by a spinlock, dropping the oldest sample when full and counting the drops | M |
| FR-04 | The driver shall register `/dev/pulsecnt` and support `read` (blocking or non-blocking), `poll`, and `ioctl` | M |
| FR-05 | The driver shall support the ioctl commands RESET, SET_WATTS (1 to 100000 W), and GET_STATS | M |
| FR-06 | The driver shall accept `watts` and `pulses_per_kwh` as module parameters and reject invalid values | S |
| FR-07 | The driver shall load and unload cleanly, leaving no timer or device behind | M |

### Agent and analytics

| ID | Requirement | Priority |
|---|---|---|
| FR-08 | The agent shall read pulses from the device using `poll()` and `read()`, and reassemble records split across reads | M |
| FR-09 | The agent shall also work with a software simulator as the pulse source, selected in the configuration file | M |
| FR-10 | The agent shall compute average power in watts over a configurable sliding time window | M |
| FR-11 | The agent shall compute cumulative energy in kWh since start and its cost at a configurable tariff | M |
| FR-12 | The agent shall track peak power | S |
| FR-13 | The agent shall raise one alert when power rises above a configurable threshold, and not repeat it until power has dropped below the threshold and risen again | M |
| FR-14 | The agent shall store every reading and alert in SQLite so history survives restarts | M |
| FR-15 | The agent shall read its settings from a `key=value` configuration file, with defaults for missing or invalid values | M |
| FR-16 | The agent shall log timestamped messages with levels to the console and a file, safely from several threads | S |
| FR-17 | The agent shall shut down cleanly on SIGINT or SIGTERM, finishing queued work first | M |
| FR-18 | In device mode, the agent shall take the meter constant from the driver rather than trusting the configuration file | S |

### Web interface

| ID | Requirement | Priority |
|---|---|---|
| FR-19 | The agent shall serve a JSON API: `GET /api/live`, `GET /api/history`, `GET /api/alerts`, and `POST /api/load` | M |
| FR-20 | The API shall validate input and return clear errors for invalid requests | M |
| FR-21 | The dashboard shall show live power, energy, cost, peak, status, a history chart, and the alert list, refreshing automatically | M |
| FR-22 | The dashboard shall let the user change the load, which is applied to the pulse source | S |
| FR-23 | A command-line tool shall show driver statistics, reset the counter, and set the load | S |

---

## 3. Non-functional requirements

| ID | Category | Requirement | How it is verified |
|---|---|---|---|
| NFR-01 | Performance | Sustain at least 100 pulses per second end to end with no pulse lost | Stage 5 load test with the driver, comparing the pulses generated with the pulses stored |
| NFR-02 | Latency | A dashboard value shall be no more than 2 seconds old | Manual check against the browser refresh interval |
| NFR-03 | Reliability | The agent shall run for at least 24 hours without crashing or growing in memory | Stage 5 soak test, with valgrind and sanitizer runs |
| NFR-04 | Reliability | After the agent is killed and restarted, earlier history shall still be shown | System test with `kill -9` |
| NFR-05 | Concurrency | No data races between threads | Thread sanitizer build, code review of every shared structure |
| NFR-06 | Safety | The driver shall never sleep in timer context, and shall never block the timer when a reader is slow | Design review, `dmesg` check under load |
| NFR-07 | Portability | Build and run on Ubuntu 22.04 and 24.04 with kernel 6.x | Build in the VM |
| NFR-08 | Maintainability | C++17, one class per responsibility, no compiler warnings with `-Wall -Wextra -Wpedantic`, clean `cppcheck` | CI-style build script |
| NFR-09 | Testability | Core logic shall be testable without the kernel driver | Unit tests using fake events and a FIFO as a fake device |
| NFR-10 | Security | Input from the network shall be validated, and the web server shall listen on localhost by default | API tests with invalid input |
| NFR-11 | Usability | The dashboard shall work in a current browser with no installation and no internet access | Manual test, with Chart.js served locally |
| NFR-12 | Documentation | Each stage shall have documents, commits, evidence, and a roadmap | Repository review |

---

## 4. System modules and deliverables

| Module | Language | Key files |
|---|---|---|
| Kernel driver | C | `driver/pulsecnt.c`, `driver/pulsecnt_ioctl.h`, `driver/Makefile` |
| Driver test tool | C | `driver/pulsecnt_ctl.c` |
| Pulse sources | C++ | `PulseSource`, `DevicePulseSource`, `SimulatedPulseSource` |
| Analytics | C++ | `AnalyticsEngine`, `EnergyCalculator` |
| Storage | C++ | `Storage` (SQLite) |
| Concurrency and support | C++ | `ThreadSafeQueue`, `SharedState`, `Config`, `Logger` |
| Web server and API | C++ | `HttpServer` using cpp-httplib and nlohmann-json |
| Dashboard | HTML, JS | `agent/web/index.html`, Chart.js |
| Build and scripts | CMake, shell | `agent/CMakeLists.txt`, `scripts/driver_load.sh`, `scripts/driver_unload.sh` |
| Tests | C++ (GoogleTest) | `agent/tests/` |

**Final deliverables:** source code, this PRD, the design document with UML diagrams (`docs/design.md`), the progress log, the test report, the Git repository with tagged stages, the final report, and the presentation with a demonstration.

---

## 5. Constraints, assumptions, and risks

**Constraints**

- Individual project with a fixed schedule of six stages.
- The kernel driver must be written in C. Everything else is C++17.
- Kernel modules can only be built and loaded in a real Linux environment, so a virtual machine is needed (the WSL2 kernel cannot build modules without a custom kernel).

**Assumptions**

- A virtual machine with Ubuntu 24.04, kernel headers installed, and 4 GB of RAM is available.
- Pulses are simulated, so exact real-world meter behaviour (contact bounce, noise) is not modelled.

**Risks**

| Risk | Impact | Mitigation |
|---|---|---|
| Kernel API differs between kernel versions | Driver fails to build | The code handles the `hrtimer_setup` change at compile time. Build on the VM early and fix errors in small steps |
| A driver bug crashes the VM | Lost time | Develop in a VM, snapshot it before testing, and load the simple hello module first |
| Hardware not available | No real meter test | Simulated timer, with the pulse source behind an interface so GPIO can be added later |
| Thread bugs (races, deadlocks) | Intermittent failures | Small queue-based design, sanitizer builds, long-run tests |
| Schedule slip | Incomplete final stage | Working prototype early, documents written as the work is done, features beyond the must-haves are optional |

---

## 6. Test approach and acceptance criteria

| Level | What | Tool |
|---|---|---|
| Unit | Energy maths, analytics window, alerts, storage, shared state, simulator, device reader (using a FIFO) | GoogleTest (26 tests so far) |
| Integration | Driver and agent together, agent and database, API and dashboard | Shell scripts with `curl` and `sqlite3` |
| System | Long runs, kill and restart, unloading the driver while the agent runs, high pulse rates | Manual scripts, `valgrind`, sanitizers |

**The project is accepted when:**

1. All must-have requirements (M) are implemented and their tests pass.
2. The demonstration shows the driver loaded, the agent running with `source=device`, and the dashboard updating live, including a load change and an alert.
3. History is still present after the agent is stopped and restarted.
4. The agent shuts down cleanly with Ctrl+C.
5. The repository contains the source, documents, UML diagrams, test report, and tagged stage releases.

---

## 7. Development plan and status

Eight-week plan. Week numbers are relative to the project start. Update the status column at the end of each stage.

| Weeks | Course stage | Work | Evidence | Status |
|---|---|---|---|---|
| 1 | 1. Introduction | Project idea, problem, scope, outcome | This PRD (sections 1) | Done |
| 1 | 2. Requirements and plan | Requirements, modules, deliverables, timeline | This PRD | Done |
| 2 | 3. Design | Architecture, UML (class, sequence, state), data structures, tools, Git setup | `docs/design.md`, Git branches | Done |
| 3 to 5 | 4. Prototype | Agent core, analytics, storage, threads, simulator, API, dashboard | Unit tests, dashboard screenshot, commits | Done |
| 5 to 6 | 4. Prototype | Kernel driver and device reader | `driver/`, `DevicePulseSource` with FIFO tests | Written. Driver build and test in the VM pending |
| 6 to 7 | 5. Testing and improvement | Driver test, integration tests, soak test, valgrind and sanitizers, fixes, performance | Test report, bug log | Not started |
| 7 | 5. Testing and improvement | Polish: skip the 0 W first reading, time-scaled chart axis, `cppcheck` and `clang-format` clean | Commits | Not started |
| 8 | 6. Final | Full demo, final report, slides, tag `v1.0` | Report, presentation, release tag | Not started |

### Requirement status

| Requirements | Status |
|---|---|
| FR-08 to FR-17, FR-19 to FR-21 | Implemented in the agent and verified by unit tests, the live dashboard, and manual runs |
| FR-18, FR-22 in device mode | Implemented. To be verified with the real driver |
| FR-01 to FR-07, FR-23 | Implemented in code. To be built and verified in the VM |
| NFR-01 to NFR-06 | To be verified in Stage 5 |
| NFR-07 to NFR-12 | Partly verified. Full check in Stage 5 |

### Git strategy

- `main` holds stable, tagged stage releases.
- `develop` is the integration branch.
- Work is done on `feature/*` branches (for example `feature/driver-pulsecnt`) and merged into `develop` when tested.
- Commit messages are prefixed by area: `driver:`, `agent:`, `docs:`.
- Each finished stage is tagged: `stage1` to `stage6`, and `v1.0` for the final release.

### Roadmap after each stage

| After | Next |
|---|---|
| Stage 2 (this document) | Design and UML |
| Stage 3 | Prototype of the agent and dashboard |
| Stage 4 | Driver verification, integration tests, soak test |
| Stage 5 | Final demonstration, report, and presentation |
| Stage 6 | Future work: real GPIO interrupt, MQTT or cloud upload, time-of-use tariffs, authentication, anomaly detection |
