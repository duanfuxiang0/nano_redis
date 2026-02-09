# NanoRedis

NanoRedis is a teaching-oriented, Redis-like in-memory database that implements a Dragonfly-inspired shared-nothing,
thread-per-core sharded architecture. It supports RESP parsing, core data-type commands, pipelining (batched writes),
TTL expiration (lazy + active), and a small set of management commands (`INFO/CONFIG/CLIENT/TIME/RANDOMKEY`).

The implementation plan and current progress are tracked in `doc/nano_redis_v1.1.md`.

## Build

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

## Run

```bash
# Single shard
./build/nano_redis_server --port=9527 --num_shards=1

# Multiple shards
./build/nano_redis_server --port=9527 --num_shards=4
```

## Quick Verification With redis-cli

```bash
redis-cli -p 9527 PING
redis-cli -p 9527 SET key value
redis-cli -p 9527 GET key

# Per-connection DB selection
redis-cli -p 9527 SELECT 1

# TTL (subset)
redis-cli -p 9527 SET k v EX 1
redis-cli -p 9527 TTL k

# Pipelining: send multiple commands in one TCP write
redis-cli -p 9527 --pipe < /dev/null
```

## Tests

```bash
./build/unit_tests
./build/unit_tests --gtest_filter=ServerFamilyTest.*
```

## Supported Commands (Subset)

### String
- `SET/GET/DEL/EXISTS/MSET/MGET`
- `INCR/DECR/INCRBY/DECRBY`
- `APPEND/STRLEN/GETRANGE/SETRANGE`
- `SELECT/DBSIZE/KEYS/FLUSHDB`
- `TYPE`
- `EXPIRE/TTL/PERSIST`
- `PING/QUIT/HELLO`
- `COMMAND` (returns command metadata)

`SET` currently supports `EX|PX` only (other Redis options are not implemented yet).

### Hash / Set / List
- Hash: `HSET/HGET/HGETALL/HDEL/HEXISTS/HLEN/HKEYS/HVALS/HINCRBY/...`
- Set: `SADD/SMEMBERS/SISMEMBER/SREM/SCARD/SUNION/SINTER/...`
- List: `LPUSH/RPUSH/LPOP/RPOP/LLEN/LINDEX/LRANGE/LTRIM/...`

### Management
- `INFO [section]` (basic `server/keyspace`)
- `CONFIG GET/SET/RESETSTAT` (limited subset)
- `CLIENT GETNAME/SETNAME/ID/INFO/LIST/KILL/PAUSE`
- `TIME`
- `RANDOMKEY`

## Architecture Notes

- Shared-nothing: each vCPU thread owns exactly one shard (`EngineShard`) and runs commands on its own data.
- Cross-shard: synchronous `EngineShardSet::Await` is used for shard-hops to preserve ownership.
- Routing: `CommandMeta` (arity/key positions/flags) drives single-key routing vs multi-key execution.
- Pipeline: responses are appended into a per-connection write buffer and flushed when input is exhausted or
  the buffered bytes exceed a threshold.
- TTL: lazy expiry is applied on reads; active expiry runs periodically to delete expired keys.

## Repository Structure

```
nano_redis/
├── include/
│   ├── command/
│   ├── core/
│   ├── protocol/
│   └── server/
├── src/
│   ├── command/
│   ├── core/
│   ├── protocol/
│   └── server/
├── tests/unit/
├── doc/
└── third_party/
```
