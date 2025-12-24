# MCP Tools Usage Guidelines

## Fast Apply (morph mcp)
- **Tool**: `edit_file`
- **When to use**: Use instead of `str_replace` or full file writes
- **Why**: Works with partial code snippets—no need to provide complete file content
- **Example**: When modifying a function, you only need to include that function in the edit

## Warp Grep (morph mcp)
- **Tool**: `warpgrep_codebase_search`
- **What it does**: A subagent that finds relevant context using semantic search
- **When to use**: At the beginning of codebase explorations to quickly locate relevant files
- **Best practices**:
  - Use for broad semantic queries, not keyword searches
  - Ask about flows, architecture, or error sources
  - Ideal questions: "Find the XYZ flow", "How does XYZ work", "Where is XYZ handled?", "Where is <error message> coming from?"

# Commands
- Build: `mkdir build && cd build && cmake .. && cmake --build .`
- Run all tests: `cd build && ctest`
- Run single test: `./tests/version_test --gtest_filter=TestSuite.TestCase`

# Code Style
- C++17, use `-Wall -Wextra -Werror`
- Apache License header at top of every file
- Include guards: `#ifndef NANO_REDIS_FILENAME_H_`
- Namespace: `nano_redis`, close with `}  // namespace nano_redis`
- Constants: `k` prefix (`kMajorVersion`, `kOk`)
- Classes: PascalCase, `public:` then `private:` sections
- Methods: lowercase (`ok()`, `code()`), static methods PascalCase (`OK()`)
- Enum class: PascalCase type, PascalCase with `k` prefix values (`StatusCode::kNotFound`)
- Error handling: Status class pattern, check `ok()` before accessing
- Testing: GoogleTest, use `TEST(Suite, Name)` macro
- Indentation: 2 spaces
- Include order: project headers first, then system headers
