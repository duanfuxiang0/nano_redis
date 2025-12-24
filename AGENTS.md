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

Fast Apply: IMPORTANT: Use `edit_file` over `str_replace` or full file writes. It works with partial code snippets—no need for full file content.

Warp Grep: warp-grep is a subagent that takes in a search string and tries to find relevant context. Best practice is to use it at the beginning of codebase explorations to fast track finding relevant files/lines. Do not use it to pin point keywords, but use it for broader semantic queries. "Find the XYZ flow", "How does XYZ work", "Where is XYZ handled?", "Where is <error message> coming from?"
