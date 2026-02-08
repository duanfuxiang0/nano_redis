# Shared-Nothing Architecture

This document describes NanoRedis's shared-nothing architecture, which is inspired by Dragonfly's design.

## Overview

NanoRedis uses a **shared-nothing, thread-per-core** architecture where:

1. **N vCPUs** (OS threads), typically one per CPU core
2. Each vCPU **owns exactly one shard** (database partition)
3. **No locks** on the data path - each shard is exclusively accessed by its owning thread
4. **Cross-shard communication via message passing** (TaskQueue)

```
┌─────────────────────────────────────────────────────────────────┐
│                       NanoRedis Process                         │
├─────────────────────────────────────────────────────────────────┤
│  vCPU 0                vCPU 1               vCPU N-1            │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐      │
│  │ TCP Accept  │      │ TCP Accept  │      │ TCP Accept  │      │
│  │ (SO_REUSEPORT)     │ (SO_REUSEPORT)     │ (SO_REUSEPORT)     │
│  ├─────────────┤      ├─────────────┤      ├─────────────┤      │
│  │ Connection  │      │ Connection  │      │ Connection  │      │
│  │ Fibers      │      │ Fibers      │      │ Fibers      │      │
│  │ (Coordinators)     │ (Coordinators)     │ (Coordinators)     │
│  ├─────────────┤      ├─────────────┤      ├─────────────┤      │
│  │ Shard 0     │      │ Shard 1     │      │ Shard N-1   │      │
│  │ (Database)  │      │ (Database)  │      │ (Database)  │      │
│  ├─────────────┤      ├─────────────┤      ├─────────────┤      │
│  │ TaskQueue   │◄────►│ TaskQueue   │◄────►│ TaskQueue   │      │
│  │ (MPMC)      │      │ (MPMC)      │      │ (MPMC)      │      │
│  └─────────────┘      └─────────────┘      └─────────────┘      │
└─────────────────────────────────────────────────────────────────┘
```

## Key Components

### ProactorPool

The `ProactorPool` class manages all vCPUs:

- Creates N OS threads, each running a Photon event loop
- Each thread hosts one `EngineShard`
- Each thread creates a TCP server with `SO_REUSEPORT` for load balancing
- Handles connection fibers that act as coordinators

### EngineShard

The `EngineShard` class is a pure data container:

- Owns a `Database` instance for storing key-value data
- Has a `TaskQueue` for receiving cross-shard requests
- Uses thread-local storage (`tlocal_shard_`) for fast access

### EngineShardSet

The `EngineShardSet` provides cross-shard communication:

- `Await(shard_id, func)`: Execute func on target shard, wait for result (fiber-friendly)
- `Add(shard_id, func)`: Fire-and-forget task to target shard

## Request Flow

### Same-Shard Request (Fast Path)

```
Client ─────► vCPU X ─────► Shard X (local)
              │                 │
              │  key belongs    │
              │  to shard X     │
              ▼                 ▼
         Connection        Direct access
         Fiber             (no message passing)
```

### Cross-Shard Request

```
Client ─────► vCPU X ─────────────► TaskQueue Y ─────► vCPU Y
              │                                           │
              │  key belongs to shard Y                   │
              │  (Y != X)                                 │
              ▼                                           ▼
         Connection                                  Shard Y
         Fiber (Coordinator)                         executes
              │                                           │
              │  Fiber suspends                           │
              │  (thread NOT blocked!)                    │
              ▼                                           │
         Wait for result ◄────────────────────────────────┘
              │
              ▼
         Send response
```

## PhotonLibOS Integration

NanoRedis uses PhotonLibOS for:

1. **Fiber scheduling**: Each connection runs in its own fiber
2. **Asynchronous I/O**: Non-blocking socket operations
3. **Event loop**: Epoll-based event engine per vCPU

Key PhotonLibOS APIs used:

- `photon::init()` / `photon::fini()`: Initialize/cleanup per-thread
- `photon::thread_create11()`: Create fibers
- `photon::net::new_tcp_socket_server()`: TCP server with connection handlers
- `photon::semaphore`: Fiber-friendly wait/notify (TaskQueue notification)

## Benefits

1. **No Contention**: Each shard is owned by exactly one thread
2. **Cache-Friendly**: Data stays on the same CPU core
3. **Linear Scalability**: Adding cores = adding throughput
4. **Fiber Efficiency**: Thousands of connections per thread without context switch overhead

## Trade-offs

1. **Cross-Shard Latency**: ~1-2μs overhead for message passing
2. **Load Imbalance**: Hot keys may overload specific shards
3. **Multi-Key Commands**: Require coordination across shards

## Configuration

```bash
./nano_redis_server --num_shards=4 --port=9527
```

- `--num_shards`: Number of shards/vCPUs (default: 8)
- `--port`: TCP port to listen on (default: 9527)
