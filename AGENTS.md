# AGENTS.md

This file provides guidelines for AI coding agents working on the nano_redis codebase.

## v1.1 Implementation Notes

- The legacy `RedisServer` code path has been removed; `ShardedServer` is used for both `--num_shards=1` and multi-shard.
- Request routing is metadata-driven via `CommandRegistry::CommandMeta` (arity/key positions/flags).
- Pipeline batching is implemented via response buffering in `Connection` and no-read parsing via `RESPParser::TryParseCommandNoRead`.
- TTL expiration is implemented via lazy expiry on reads + an active expire cycle on each shard.
- Management commands live in `ServerFamily` (`INFO/CONFIG/CLIENT/TIME/RANDOMKEY`).

## Project Structure

The project has a simple structure with three main directories:

* **include/** - Header files organized by module (command, protocol, server, core)
* **src/** - Source files corresponding to the header files
* **tests/unit/** - Unit tests for the core components

## External Reference Codebases

* **./dragonfly** - A reference library located in the root directory for learning purposes. All code in this repository can be read and studied.
* **./valkey** - A reference library located in the root directory for learning purposes. All code in this repository can be read and studied.
* **./third_party/photonlibos** - A dependent library in the third_party directory. Focus on reading and learning its interfaces and APIs.

## Build, Lint, and Test Commands

### Build
```bash
# Debug
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Release
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run All Tests
```bash
./build/unit_tests
```

### Run Single Test
```bash
./build/unit_tests --gtest_filter=TestSuite.TestName
```

### Common Gotchas

- If you remove/rename source files, re-run `cmake -S . -B build ...` to regenerate build files (stale builds may still
  reference deleted paths).
- Prefer `git commit -m "..."` to avoid interactive editors in automated environments.

### Lint/Format
```bash
# Format code (Google style)
clang-format -i --style=file <file>
```

## C++ Guidelines

* Do not use `malloc`, prefer the use of smart pointers. Keywords `new` and `delete` are a code smell.
* Strongly prefer the use of `unique_ptr` over `shared_ptr`, only use `shared_ptr` if you **absolutely** have to.
* Use `const` whenever possible.
* Do **not** import namespaces (e.g. `using std`).
* When overriding a virtual method, avoid repeating virtual and always use `override` or `final`.
* Use `[u]int(8|16|32|64)_t` instead of `int`, `long`, `uint` etc. Use `idx_t` instead of `size_t` for offsets/indices/counts of any kind.
* Prefer using references over pointers as arguments.
* Use `const` references for arguments of non-trivial objects (e.g. `std::vector`, ...).
* Use C++11 for loops when possible: `for (const auto& item : items) {...}`
* Use braces for indenting `if` statements and loops. Avoid single-line if statements and loops, especially nested ones.
* **Class Layout:** Start out with a `public` block containing the constructor and public variables, followed by a `public` block containing public methods of the class. After that follow any private functions and private variables. For example:
    ```cpp
    class MyClass {
    public:
    	MyClass();

    	int my_public_variable;

    public:
    	void MyFunction();

    private:
    	void MyPrivateFunction();

    private:
    	int my_private_variable;
    };
    ```
* Avoid [unnamed magic numbers](https://en.wikipedia.org/wiki/Magic_number_(programming)). Instead, use named variables that are stored in a `constexpr`.
* [Return early](https://medium.com/swlh/return-early-pattern-3d18a41bba8). Avoid deep nested branches.
* Do not include commented out code blocks in pull requests.

## Naming Conventions

* Choose descriptive names. Avoid single-letter variable names.
* Files: lowercase separated by underscores, e.g., abstract_operator.cpp
* Types (classes, structs, enums, typedefs, using): CamelCase starting with uppercase letter, e.g., BaseColumn
* Variables: lowercase separated by underscores, e.g., chunk_size
* Functions: CamelCase starting with uppercase letter, e.g., GetChunk
* Avoid `i`, `j`, etc. in **nested** loops. Prefer to use e.g. **column_idx**, **check_idx**. In a **non-nested** loop it is permissible to use **i** as iterator index.
* These rules are partially enforced by `clang-tidy`.

## Build System Notes

- C++17 standard required
- Compiler warnings: `-Wall -Wextra`
- No error from warnings (`-Wno-error`)
- Build produces: `nano_redis_server` and `unit_tests`

## Shared-Nothing Architecture

This project uses a shared-nothing, thread-per-core architecture inspired by Dragonfly.

**See `doc/shared_nothing_architecture.md` for full details.**

Key concepts:
- N vCPUs (threads), each owning exactly one shard
- Connection Fibers act as coordinators
- Cross-shard requests via TaskQueue message passing (no shared mutable state)
- I/O distributed via SO_REUSEPORT

Key components:
- `ProactorPool`: Manages all vCPUs and connection handling
- `EngineShard`: Pure data container (Database + TaskQueue)
- `EngineShardSet`: Cross-shard communication (Await/Add)
