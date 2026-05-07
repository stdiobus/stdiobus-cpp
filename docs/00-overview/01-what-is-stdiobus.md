# What is stdio_bus?

## Purpose

stdio_bus is a **deterministic transport layer** for AI agent protocols (ACP/MCP). It provides:

- **Process supervision**: Manages worker process lifecycle (spawn, monitor, restart)
- **Message routing**: Routes JSON-RPC messages between clients and workers
- **Session management**: Maintains session affinity for stateful conversations
- **Backpressure control**: Prevents memory exhaustion under load

## What stdio Bus is NOT

- ✘ Not an AI/ML framework
- ✘ Not a message queue (no persistence)
- ✘ Not a protocol implementation (protocol-agnostic)
- ✘ Not multi-threaded (single event loop by design)

## Architecture

```mermaid
sequenceDiagram
    participant App as Host Application
    participant Bus as Bus / AsyncBus / BusBuilder
    participant FFI as FFI Layer
    participant Kernel as libstdio_bus.a (C)
    participant W1 as Worker 1 (ACP)
    participant W2 as Worker 2 (ACP)
    participant WN as Worker N (ACP)

    App->>Bus: High-level API call
    Bus->>FFI: Translated C++ → C
    FFI->>Kernel: stdio_bus_* function

    par Worker 1 communication
        Kernel->>W1: stdin pipe (JSON-RPC)
        W1->>Kernel: stdout pipe (response)
    and Worker 2 communication
        Kernel->>W2: stdin pipe (JSON-RPC)
        W2->>Kernel: stdout pipe (response)
    and Worker N communication
        Kernel->>WN: stdin pipe (JSON-RPC)
        WN->>Kernel: stdout pipe (response)
    end

    Kernel-->>FFI: callback
    FFI-->>Bus: std::function trampoline
    Bus-->>App: on_message(string_view)
```

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Single-threaded | Deterministic behavior, no race conditions |
| Non-blocking I/O | Responsive event loop, no deadlocks |
| No external deps | Minimal attack surface, easy embedding |
| Protocol agnostic | Forward messages unchanged, parse only routing fields |
| NDJSON framing | Simple, debuggable, streaming-friendly |

## Message Flow

```mermaid
sequenceDiagram
    participant Client
    participant Ingest
    participant Router as Route
    participant Queue
    participant Worker
    participant Correlator as Correlate
    participant Callback

    Client->>Ingest: Request (JSON-RPC)
    Note over Ingest: Parse sessionId, id
    Ingest->>Router: Parsed envelope
    Note over Router: Session → Worker<br/>mapping
    Router->>Queue: Routed message
    Queue->>Worker: stdin (NDJSON)
    Note over Worker: Process request
    Worker->>Correlator: stdout (NDJSON)
    Note over Correlator: Match response.id
    Correlator->>Callback: Matched response
    Callback-->>Client: Response delivered
```

## When to Use stdio_bus

✓ **Good fit:**
- Local AI tool runtime (MCP servers)
- Agent orchestration with session state
- High-throughput message routing
- Cross-language worker pools
- Deterministic replay/audit requirements

✘ **Not ideal for:**
- Distributed multi-host deployments (use TCP mode with care)
- Persistent message queuing (use Kafka/RabbitMQ)
- Request/response with >10s latency (use async patterns)
