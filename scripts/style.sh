#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./scripts/style.sh format [paths...]
  ./scripts/style.sh check  [paths...]
  ./scripts/style.sh tidy   [--build-dir DIR] [--fix] [paths...]

Notes:
  - Only processes tracked files under include/, src/, tests/ by default.
  - Always excludes third_party/, dragonfly/, valkey/, build/, tests/draft/.
  - 'tidy' runs clang-tidy with --quiet by default (reduces summary noise).
  - 'paths' are git pathspecs (e.g. 'src', 'src/server', 'tests/unit/resp_parser_test.cc').
EOF
}

repo_root() {
  git rev-parse --show-toplevel 2>/dev/null
}

require_cmd() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "error: '${cmd}' not found in PATH" >&2
    return 1
  fi
}

clang_tidy_cmd() {
  if command -v clang-tidy >/dev/null 2>&1; then
    echo "clang-tidy"
    return 0
  fi
  if command -v clang-tidy-18 >/dev/null 2>&1; then
    echo "clang-tidy-18"
    return 0
  fi
  if command -v clang-tidy-17 >/dev/null 2>&1; then
    echo "clang-tidy-17"
    return 0
  fi
  return 1
}

excluded_prefix_re='^(third_party|dragonfly|valkey|build|tests/draft)/'
cpp_file_re='\.(c|cc|cpp|cxx|h|hh|hpp|tcc)$'

list_files_z() {
  local -a pathspecs=("$@")

  if [[ ${#pathspecs[@]} -eq 0 ]]; then
    pathspecs=("include" "src" "tests")
  fi

  git ls-files -z -- "${pathspecs[@]}" \
    | rg --null-data -v "${excluded_prefix_re}" \
    | rg --null-data "${cpp_file_re}" || true
}

run_clang_format() {
  local mode="$1"
  shift
  require_cmd clang-format
  require_cmd rg
  require_cmd git

  local root
  root="$(repo_root)"
  if [[ -z "${root}" ]]; then
    echo "error: not inside a git repository" >&2
    return 1
  fi
  cd "${root}"

  local -a files=()
  while IFS= read -r -d '' file; do
    files+=("${file}")
  done < <(list_files_z "$@")

  if [[ ${#files[@]} -eq 0 ]]; then
    echo "no matching C/C++ files (tracked, under include/src/tests; excluding third_party)" >&2
    return 0
  fi

  echo "clang-format (${mode}) on ${#files[@]} files"

  if [[ "${mode}" == "check" ]]; then
    clang-format --dry-run --Werror --style=file "${files[@]}"
  else
    clang-format -i --style=file "${files[@]}"
  fi
}

ensure_compile_commands() {
  local build_dir="$1"
  if [[ -f "${build_dir}/compile_commands.json" ]]; then
    return 0
  fi
  cmake -S . -B "${build_dir}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
}

run_clang_tidy() {
  require_cmd rg
  require_cmd git
  require_cmd cmake

  local tidy_bin
  if ! tidy_bin="$(clang_tidy_cmd)"; then
    cat >&2 <<'EOF'
error: clang-tidy not found (try installing clang-tidy).
tip: on Ubuntu: sudo apt-get install clang-tidy
EOF
    return 1
  fi

  local build_dir="build"
  local fix="0"
  local -a pathspecs=()

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --build-dir)
        build_dir="$2"
        shift 2
        ;;
      --fix)
        fix="1"
        shift
        ;;
      -h|--help)
        usage
        return 0
        ;;
      --)
        shift
        pathspecs+=("$@")
        break
        ;;
      *)
        pathspecs+=("$1")
        shift
        ;;
    esac
  done

  local root
  root="$(repo_root)"
  if [[ -z "${root}" ]]; then
    echo "error: not inside a git repository" >&2
    return 1
  fi
  cd "${root}"

  ensure_compile_commands "${build_dir}"

  local -a files=()
  while IFS= read -r -d '' file; do
    files+=("${file}")
  done < <(list_files_z "${pathspecs[@]}")

  if [[ ${#files[@]} -eq 0 ]]; then
    echo "no matching C/C++ files (tracked, under include/src/tests; excluding third_party)" >&2
    return 0
  fi

  echo "${tidy_bin} on ${#files[@]} files (build dir: ${build_dir})"

  local tidy_extra=()
  if [[ "${fix}" == "1" ]]; then
    tidy_extra=(--fix --fix-errors)
  fi

  for file in "${files[@]}"; do
    "${tidy_bin}" --quiet -p "${build_dir}" "${tidy_extra[@]}" "${file}"
  done
}

main() {
  local cmd="${1:-format}"
  shift || true

  case "${cmd}" in
    -h|--help|help)
      usage
      ;;
    format)
      run_clang_format "format" "$@"
      ;;
    check)
      run_clang_format "check" "$@"
      ;;
    tidy)
      run_clang_tidy "$@"
      ;;
    *)
      echo "error: unknown command: ${cmd}" >&2
      usage >&2
      return 2
      ;;
  esac
}

main "$@"
