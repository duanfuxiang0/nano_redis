# NanoRedis v1.1 Roadmap (Updated)

This file tracks the original v1.1 roadmap plus the current completion status and remaining work.

## Status (As Of 2026-02-10)

- Phase 1: completed
- Phase 2: completed except 2.5 (NanoObj alias cleanup)
- Phase 3: completed for 3.1/3.2(core subset)/3.3(core subset)/3.4, with remaining items listed below
- Tests: `./build/unit_tests` passing (211 tests)

## Remaining Work (Visible Expectations)

### Phase 2

- [ ] 2.5 NanoObj alias cleanup
  - Goal: remove transitional lowercase aliases and `NOLINTNEXTLINE` wrappers, keep only CamelCase APIs.
  - Acceptance:
    - No remaining calls to the lowercase alias methods across `src/` and `tests/`.
    - `cmake --build build -j` and `./build/unit_tests` pass.

### Phase 3

- [ ] 3.2 TTL/expiration (command surface completion)
  - Implement missing commands: `PEXPIRE/PTTL/EXPIREAT/PEXPIREAT` (and optionally `PERSIST` is already present).
  - Extend `SET` options: `NX/XX/EXAT/PXAT` (currently only `EX|PX` are supported).
  - Acceptance:
    - Unit tests for each new command and edge cases (negative TTL, overflow, missing keys).
    - Behavior matches Redis-like return conventions (`TTL`: -2 for missing key, -1 for no expire).

- [ ] 3.3 INFO/CONFIG (metrics completeness)
  - INFO sections are currently minimal (`server`, `keyspace`). Remaining roadmap sections:
    - `clients`: current connection count
    - `memory`: allocator stats (optional)
    - `shards`: per-shard key distribution (optional)
  - CONFIG currently supports a limited subset; the roadmap mentions `hz` (active-expiry frequency) which is not yet
    wired to the running expiry loop.
  - Acceptance:
    - `INFO clients` returns a stable count.
    - If `hz` is implemented: changing it affects the active expiry scheduling.

- [ ] CLIENT command surface (optional expansion)
  - Currently supported: `GETNAME/SETNAME/ID/INFO/LIST/KILL/PAUSE`.
  - Redis has a larger matrix (e.g. `KILL` by addr/type, `SKIPME`, etc.) which are not implemented.

## Phase 1 — Bug Fixes & Code Hygiene (Completed)

- [x] 1.1 CMakeLists.txt de-duplication
- [x] 1.2 FLUSHDB uses `Await()` in sharded mode (synchronous)
- [x] 1.3 KEYS aggregates across shards
- [x] 1.4 Integer parsing returns `std::optional<int64_t>` and callers return proper errors
- [x] 1.5 Hygiene: removed unused includes/unreachable code; unified constants; added DashTable rationale comment

## Phase 2 — Architecture Improvements (Completed Except 2.5)

- [x] 2.1 Per-connection state
  - `Connection` owns `db_index`; `SELECT` updates per-connection state, not global DB state.
  - `CommandContext` carries `db_index` and uses it to select the correct DB view.
- [x] 2.2 CommandMeta
  - Command registration includes arity/key positions/flags.
  - `ProactorPool` routes single-key commands to the owning shard using the meta.
- [x] 2.3 Unified server
  - Legacy `RedisServer` code path removed; `ShardedServer` handles `--num_shards=1` and multi-shard.
- [x] 2.4 Connection integration
  - `Connection` is the per-client object: socket, parser, response buffering, per-connection state.
- [ ] 2.5 NanoObj alias cleanup (remaining)
- [x] 2.6 Deterministic sharding hash (replaced `std::hash` usage)
- [x] 2.7 Type-safe `CommandContext::connection` (`Connection*`)

## Phase 3 — New Features (Completed Core Subset)

### 3.1 Pipeline (Completed)

- [x] Response buffering in `Connection`
- [x] No-read parsing attempt: `RESPParser::TryParseCommandNoRead`
- [x] Proactor logic: block for first command, then batch parse from buffered bytes and flush on `NEED_MORE`

### 3.2 TTL / Expiration (Core Subset Completed)

- [x] Per-DB expire table and APIs
- [x] Lazy expiry (read paths prune expired keys)
- [x] Active expiry (periodic cycle in each shard)
- [x] Commands: `EXPIRE`, `TTL`, `PERSIST`
- [x] `SET` supports `EX|PX`
- [ ] Missing command surface (see Remaining Work)

### 3.3 INFO / CONFIG / CLIENT / TIME / RANDOMKEY (Core Subset Completed)

- [x] `INFO` (basic `server` + `keyspace`)
- [x] `CONFIG GET/SET/RESETSTAT` (limited subset)
- [x] `CLIENT GETNAME/SETNAME/ID/INFO/LIST/KILL/PAUSE` (subset)
- [x] `TIME`
- [x] `RANDOMKEY`

### 3.4 TYPE (Completed)

- [x] `TYPE` returns Redis-like type strings (`none/string/hash/set/list`)

## Acceptance Checklist (Updated)

- [x] Build succeeds: `cmake --build build -j`
- [x] Unit tests pass: `./build/unit_tests`
- [ ] Benchmark goals (optional): `redis-benchmark -P 16` shows pipeline throughput improvement (not automated here)
