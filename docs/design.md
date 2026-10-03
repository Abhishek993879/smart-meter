# System Design: Smart Energy Smart-Meter Pulse Counter & Analytics Agent

This document describes the architecture and design of the project. The diagrams are written in Mermaid, so GitHub draws them when you open this file in the repository.

## 1. Architecture

A Linux kernel module simulates a meter's pulse output. A C++ daemon reads the pulses, computes power and energy, stores the results in SQLite, and serves a web dashboard.

```mermaid
flowchart LR
    subgraph K["Kernel space (C)"]
        T["hrtimer<br/>(simulated meter pulses)"]
        R["Ring buffer<br/>256 samples + spinlock"]
        D["/dev/pulsecnt<br/>read, poll, ioctl"]
        T --> R --> D
    end

    subgraph A["energy-agent (C++17)"]
        S["PulseSource<br/>Device or Simulated"]
        Q1["Event queue"]
        AN["AnalyticsEngine"]
        Q2["Reading queue"]
        ST["Storage thread"]
        SS["SharedState<br/>newest reading"]
        H["HttpServer<br/>JSON API + static files"]
        DB[("SQLite<br/>readings, alerts")]
        S --> Q1 --> AN
        AN --> Q2 --> ST --> DB
        AN --> SS
        SS --> H
        DB --> H
    end

    D -- "poll + read" --> S
    H -- "ioctl set load" --> D

    B["Browser dashboard<br/>HTML + JavaScript + Chart.js"]
    B -- "GET /api/live, /api/history, /api/alerts" --> H
    B -- "POST /api/load" --> H
```

### Components and responsibilities

| Component | Language | Responsibility |
|---|---|---|
| `pulsecnt` kernel module | C | Generates timestamped pulses with an hrtimer, buffers them, and exposes them through `/dev/pulsecnt` (`read`, `poll`, `ioctl`) |
| `PulseSource` (`DevicePulseSource`, `SimulatedPulseSource`) | C++ | Delivers `PulseEvent` records. The device version reads the driver, the simulated version needs no kernel module |
| `ThreadSafeQueue<T>` | C++ | Passes data between threads without data races, and supports a clean stop |
| `AnalyticsEngine` | C++ | Keeps a sliding time window and computes power (W), energy (kWh), peak power, and the alert flag |
| `EnergyCalculator` | C++ | Pure conversion functions: pulses to kWh, pulses over time to watts, kWh to cost |
| `Storage` | C++ | Saves readings and alerts in SQLite, and reads the latest rows back |
| `SharedState` | C++ | Holds the newest reading for the live API, protected by a mutex |
| `HttpServer` | C++ | Serves the dashboard files and the JSON API on its own thread |
| `Config`, `Logger` | C++ | Read `agent.conf`, and write timestamped log lines from any thread |
| Dashboard | HTML/JS | Shows live power, energy, cost, peak, status, a history chart, and alerts, and lets the user change the load |

### Threads in the agent

| Thread | Work |
|---|---|
| Reader | Waits for pulses with `poll()`, reads them, pushes them into the event queue |
| Analytics | Takes events from the queue, computes a reading, publishes it to `SharedState`, pushes it to the reading queue |
| Storage | Takes readings from the queue, writes them to SQLite, and records an alert when one starts |
| HTTP | Answers browser requests |
| Main | Waits for Ctrl+C or SIGTERM, then stops everything in pipeline order |

## 2. Data structures

| Structure | Fields | Used for |
|---|---|---|
| `pulsecnt_sample` (kernel and agent) | `timestamp_ns`, `count` (both 64-bit) | One pulse as returned by `read()` |
| `pulsecnt_stats` | `count`, `dropped`, `watts`, `pulses_per_kwh` | Result of the `GET_STATS` ioctl |
| `PulseEvent` | `timestamp_ns`, `count` | One pulse inside the agent |
| `Reading` | `timestamp_ns`, `watts`, `kwhTotal`, `alert` | Output of the analytics engine |
| `LiveState` | `hasData`, `timestamp_ns`, `watts`, `kwhTotal`, `peakWatts`, `alert` | Newest reading for `/api/live` |
| Driver ring buffer | 256 `pulsecnt_sample` slots, free-running head and tail indices | Holds pulses until userspace reads them. When it is full, the oldest sample is dropped and counted |
| SQLite table `readings` | `id`, `ts_ns`, `watts`, `kwh_total` | History for the chart |
| SQLite table `alerts` | `id`, `ts_ns`, `message` | Alert list |

### Driver interface

| Operation | Behaviour |
|---|---|
| `read()` | Returns one or more 16-byte samples. Blocks until a pulse exists, unless the file is opened non-blocking |
| `poll()` | Reports the device readable when at least one pulse is waiting |
| `ioctl RESET` | Clears the counter and the buffer |
| `ioctl SET_WATTS` | Changes the simulated load (1 to 100000 W) and re-arms the timer |
| `ioctl GET_STATS` | Returns count, dropped samples, load, and the meter constant |
| Module parameters | `watts` (default 1500) and `pulses_per_kwh` (default 3600) |

The interval between pulses is `3.6e15 / (watts x pulses_per_kwh)` nanoseconds. For example, 1000 W at 3600 pulses per kWh gives one pulse per second.

## 3. Class diagram (C++ agent)

```mermaid
classDiagram
    class PulseSource {
        <<interface>>
        +next(event) bool
        +setLoad(watts) bool
    }
    class SimulatedPulseSource {
        -watts_ : atomic double
        -pulsesPerKwh_ : int
        +next(event) bool
        +setLoad(watts) bool
    }
    class DevicePulseSource {
        -fd_ : int
        -partial_ : vector
        -ready_ : deque
        +isOpen() bool
        +next(event) bool
        +setLoad(watts) bool
        +readDriverSettings(watts, ppk) bool
    }
    class AnalyticsEngine {
        -window_ : deque of PulseEvent
        -peakWatts_ : double
        +update(event) Reading
        +peakWatts() double
    }
    class EnergyCalculator {
        <<namespace>>
        +pulsesToKwh(pulses, ppk) double
        +averageWatts(pulses, seconds, ppk) double
        +cost(kwh, tariff) double
    }
    class Storage {
        -db_ : sqlite3 pointer
        -mutex_ : mutex
        +open(path) bool
        +insertReading(ts, watts, kwh) bool
        +insertAlert(ts, message) bool
        +latestReadings(limit) vector
        +latestAlerts(limit) vector
    }
    class SharedState {
        -state_ : LiveState
        -mutex_ : mutex
        +set(state)
        +get() LiveState
    }
    class ThreadSafeQueue~T~ {
        -queue_ : queue of T
        -mutex_ : mutex
        -cv_ : condition_variable
        +push(value)
        +waitPop() optional of T
        +stop()
    }
    class HttpServer {
        -settings_ : ServerSettings
        +start() bool
        +stop()
    }
    class Config {
        +load(path) bool
        +getString(key, def) string
        +getInt(key, def) int
        +getDouble(key, def) double
    }
    class Logger {
        <<singleton>>
        +instance() Logger
        +log(level, message)
    }
    class PulseEvent {
        <<struct>>
        +timestamp_ns : uint64
        +count : uint64
    }
    class Reading {
        <<struct>>
        +timestamp_ns : uint64
        +watts : double
        +kwhTotal : double
        +alert : bool
    }

    PulseSource <|-- SimulatedPulseSource
    PulseSource <|-- DevicePulseSource
    PulseSource ..> PulseEvent : produces
    AnalyticsEngine ..> EnergyCalculator : uses
    AnalyticsEngine ..> PulseEvent : consumes
    AnalyticsEngine ..> Reading : produces
    HttpServer --> Storage : reads history
    HttpServer --> SharedState : reads live data
    HttpServer ..> PulseSource : changes load
    Storage ..> Logger : logs errors
    DevicePulseSource ..> Logger : logs errors
```

## 4. Sequence diagram (one pulse, end to end)

```mermaid
sequenceDiagram
    autonumber
    participant Timer as Kernel hrtimer
    participant Ring as Driver ring buffer
    participant Dev as DevicePulseSource (reader thread)
    participant EQ as Event queue
    participant An as AnalyticsEngine (analytics thread)
    participant SS as SharedState
    participant RQ as Reading queue
    participant St as Storage (storage thread)
    participant Web as HttpServer
    participant Br as Browser

    Timer->>Ring: store sample (timestamp, count)
    Timer-->>Dev: wake up waiting reader
    Dev->>Ring: poll then read
    Ring-->>Dev: pulsecnt_sample
    Dev->>EQ: push PulseEvent
    EQ->>An: waitPop
    An->>An: update sliding window, compute watts and kWh
    An->>SS: set newest reading
    An->>RQ: push Reading
    RQ->>St: waitPop
    St->>St: insert reading, insert alert if one starts

    Br->>Web: GET /api/live
    Web->>SS: get
    SS-->>Web: LiveState
    Web-->>Br: JSON
    Br->>Web: GET /api/history
    Web->>St: latestReadings
    St-->>Web: rows
    Web-->>Br: JSON
```

### Changing the load from the dashboard

```mermaid
sequenceDiagram
    autonumber
    participant Br as Browser
    participant Web as HttpServer
    participant Dev as DevicePulseSource
    participant Drv as pulsecnt driver

    Br->>Web: POST /api/load with watts
    Web->>Web: validate range 100 to 10000
    Web->>Dev: setLoad(watts)
    Dev->>Drv: ioctl SET_WATTS
    Drv->>Drv: update load, re-arm timer
    Drv-->>Dev: success
    Dev-->>Web: true
    Web-->>Br: ok true
```

## 5. State machine diagrams

### Agent lifecycle

```mermaid
stateDiagram-v2
    [*] --> Starting
    Starting --> Failed : config, database, source or port error
    Starting --> Running : all components started
    Running --> ShuttingDown : SIGINT, SIGTERM, or pulse source ended
    ShuttingDown --> Stopped : web server, reader, analytics, storage threads joined in order
    Failed --> [*]
    Stopped --> [*]
```

### Load status (alerts)

```mermaid
stateDiagram-v2
    [*] --> Normal
    Normal --> HighLoad : power above alert threshold (record one alert)
    HighLoad --> HighLoad : power still above threshold (no new alert)
    HighLoad --> Normal : power back below threshold
```

### Driver lifecycle

```mermaid
stateDiagram-v2
    [*] --> Unloaded
    Unloaded --> Generating : insmod, device registered, timer started
    Generating --> Generating : timer fires, sample stored, readers woken
    Generating --> Generating : ioctl SET_WATTS, timer re-armed
    Generating --> Unloaded : rmmod, device removed, timer cancelled
```

## 6. Shutdown order

The agent stops its components from the front of the pipeline to the back, so nothing is lost and no thread waits forever:

1. The web server stops, so no new requests arrive.
2. The reader thread stops, because its `next()` call sees the shutdown flag.
3. The event queue is stopped and the analytics thread drains it and ends.
4. The reading queue is stopped and the storage thread drains it and ends.

## 7. Design decisions

- **One interface for pulse sources.** The agent does not care whether pulses come from the driver or the simulator. This allowed the whole pipeline to be built and tested before the driver existed.
- **Queues between threads.** Each thread has one job, and a slow database write never delays pulse reading.
- **Pure functions for the maths.** `EnergyCalculator` has no state and no I/O, so it is easy to unit test.
- **Shared state for live data, SQLite for history.** Live values are served from memory without touching the disk. History survives restarts.
- **Driver ring buffer with drop-oldest.** A slow reader can never block the timer in interrupt context. Lost samples are counted in `dropped`.
- **The driver is authoritative for the meter constant.** In device mode the agent reads `pulses_per_kwh` from the driver, so a config mismatch cannot produce wrong energy values.

## 8. Known limitations

- The meter is simulated. The driver's timer replaces a real GPIO interrupt.
- The device node is world-accessible (mode 0666), which is acceptable for a demonstration but not for production.
- Power needs at least two pulses inside the averaging window, so the first reading after a start is 0 W.
- The dashboard chart uses text labels, so gaps in time are not drawn to scale.
