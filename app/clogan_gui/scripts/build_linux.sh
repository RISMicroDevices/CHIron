#!/usr/bin/env bash
set -euo pipefail

build_type="Release"
build_dir="build-linux"
generator=""
qt_root="${CMAKE_PREFIX_PATH:-}"
qt6_dir="${Qt6_DIR:-}"
cmake_path=""
cmake_path_from_env=""
deploy_qt="ON"
deploy_qt_explicit=""
no_benchmarks="OFF"
clean_build="OFF"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "${script_dir}/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: ./scripts/build_linux.sh [options]

Options:
  --build-type <type>   Debug, Release, RelWithDebInfo, or MinSizeRel.
  --build-dir <path>    Build directory relative to app/clogan_gui or absolute.
  --generator <name>    CMake generator. Defaults to Ninja when available.
  --qt-root <path>      Qt prefix such as ~/Qt/6.11.0/gcc_64.
  --qt6-dir <path>      Direct path to Qt6's CMake package directory.
  --deploy-qt           Deploy the Qt runtime into the staged release directory.
  --no-deploy-qt        Skip Qt runtime deployment for local-only builds.
  --no-benchmarks       Disable the loader benchmark target at configure time.
  --clean               Remove the build directory before configuring.
  --help                Show this help text.
EOF
}

abs_path() {
  local path_value="$1"
  if [[ "$path_value" = /* ]]; then
    printf '%s\n' "$path_value"
  else
    printf '%s\n' "${project_root}/${path_value}"
  fi
}

find_command_path() {
  local command_name="$1"
  command -v "$command_name" 2>/dev/null || true
}

resolve_qt_prefix_from_qt6_dir() {
  local resolved_path
  resolved_path="$(abs_path "$1")"
  for _ in 1 2 3; do
    resolved_path="$(dirname "$resolved_path")"
  done
  printf '%s\n' "$resolved_path"
}

find_cmake_path() {
  local candidate=""
  local candidate_root=""
  local qt_tools_root=""
  local -a candidate_roots=()

  candidate="$(find_command_path cmake)"
  if [[ -n "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  if [[ -n "$qt_root" ]]; then
    candidate_roots+=("$qt_root")
  fi

  if [[ -n "$qt6_dir" ]]; then
    candidate_roots+=("$(resolve_qt_prefix_from_qt6_dir "$qt6_dir")")
  fi

  for candidate_root in "${candidate_roots[@]}"; do
    qt_tools_root="$(dirname "$(dirname "$candidate_root")")"
    for candidate in \
      "${qt_tools_root}/Tools/CMake/bin/cmake" \
      "${qt_tools_root}/Tools/CMake_64/bin/cmake" \
      "${qt_tools_root}/Tools/CMake/CMake.app/Contents/bin/cmake"
    do
      if [[ -x "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
  done

  for candidate in \
    /usr/bin/cmake \
    /usr/local/bin/cmake \
    /snap/bin/cmake \
    /opt/homebrew/bin/cmake \
    /Applications/CMake.app/Contents/bin/cmake
  do
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

run() {
  printf '\n> %s\n' "$*"
  "$@"
}

get_cmake_cache_value() {
  local cache_file="$1"
  local variable_name="$2"

  if [[ ! -f "$cache_file" ]]; then
    return 1
  fi

  grep -E "^${variable_name}:[^=]*=" "$cache_file" | head -n 1 | sed -E 's/^[^=]*=//'
}

cleanup_legacy_release_artifacts() {
  local build_root="$1"

  rm -f \
    "${build_root}/CloganGUI" \
    "${build_root}/CloganGUILoaderBench"

  rm -rf \
    "${build_root}/x64/bin" \
    "${build_root}/x64/lib" \
    "${build_root}/x86/bin" \
    "${build_root}/x86/lib"

  rmdir "${build_root}/x64" "${build_root}/x86" 2>/dev/null || true
}

invoke_qt_deployment() {
  local build_root="$1"
  local release_root="$2"
  local cache_file="${build_root}/CMakeCache.txt"
  local deploy_script

  deploy_script="$(get_cmake_cache_value "$cache_file" "CHIRON_GUI_DEPLOY_SCRIPT" || true)"
  if [[ -z "$deploy_script" || ! -f "$deploy_script" ]]; then
    printf 'Qt deployment script was not generated. Reconfigure with deployment enabled or pass --no-deploy-qt.\n' >&2
    exit 1
  fi

  run "$cmake_path" \
    -DQT_DEPLOY_PREFIX="$release_root" \
    -DQT_DEPLOY_BIN_DIR=. \
    -DQT_DEPLOY_LIB_DIR=. \
    -DQT_DEPLOY_PLUGINS_DIR=. \
    -DQT_DEPLOY_TRANSLATIONS_DIR=translations \
    -P "$deploy_script"
}

assert_qt_deployment() {
  local release_root="$1"
  local missing=()

  for pattern in \
    "libQt6Core.so*" \
    "libQt6Gui.so*" \
    "libQt6Widgets.so*"
  do
    if ! compgen -G "${release_root}/${pattern}" >/dev/null; then
      missing+=("$pattern")
    fi
  done

  if [[ ! -f "${release_root}/qt.conf" ]]; then
    missing+=("qt.conf")
  fi

  if [[ ! -d "${release_root}/platforms" ]] || ! compgen -G "${release_root}/platforms/*" >/dev/null; then
    missing+=("platforms/*")
  fi

  if (( ${#missing[@]} > 0 )); then
    printf 'Qt deployment is incomplete under %s. Missing: %s\n' "$release_root" "${missing[*]}" >&2
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-type)
      build_type="$2"
      shift 2
      ;;
    --build-dir)
      build_dir="$2"
      shift 2
      ;;
    --generator)
      generator="$2"
      shift 2
      ;;
    --qt-root)
      qt_root="$2"
      shift 2
      ;;
    --qt6-dir)
      qt6_dir="$2"
      shift 2
      ;;
    --deploy-qt)
      if [[ "$deploy_qt_explicit" = "OFF" ]]; then
        printf 'Pass either --deploy-qt or --no-deploy-qt, not both.\n' >&2
        exit 1
      fi
      deploy_qt="ON"
      deploy_qt_explicit="ON"
      shift
      ;;
    --no-deploy-qt)
      if [[ "$deploy_qt_explicit" = "ON" ]]; then
        printf 'Pass either --deploy-qt or --no-deploy-qt, not both.\n' >&2
        exit 1
      fi
      deploy_qt="OFF"
      deploy_qt_explicit="OFF"
      shift
      ;;
    --no-benchmarks)
      no_benchmarks="ON"
      shift
      ;;
    --clean)
      clean_build="ON"
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown option: %s\n' "$1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

case "$build_type" in
  Debug|Release|RelWithDebInfo|MinSizeRel)
    ;;
  *)
    printf 'Unsupported build type: %s\n' "$build_type" >&2
    exit 1
    ;;
esac

cmake_path_from_env="$(find_command_path cmake)"

if [[ -z "$qt_root" && -z "$qt6_dir" ]]; then
  if command -v qtpaths6 >/dev/null 2>&1; then
    qt_root="$(qtpaths6 --query QT_INSTALL_PREFIX 2>/dev/null || true)"
  elif command -v qtpaths >/dev/null 2>&1; then
    qt_root="$(qtpaths --query QT_INSTALL_PREFIX 2>/dev/null || true)"
  elif command -v qmake6 >/dev/null 2>&1; then
    qt_root="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  elif command -v qmake >/dev/null 2>&1; then
    qt_root="$(qmake -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  fi
fi

if [[ -n "$qt_root" ]]; then
  qt_root="$(abs_path "$qt_root")"
  export PATH="${qt_root}/bin:${PATH}"
fi

if [[ -n "$qt6_dir" ]]; then
  qt6_dir="$(abs_path "$qt6_dir")"
fi

if [[ -z "$qt_root" && -n "$qt6_dir" ]]; then
  qt_root="$(resolve_qt_prefix_from_qt6_dir "$qt6_dir")"
fi

if [[ -z "$generator" ]] && command -v ninja >/dev/null 2>&1; then
  generator="Ninja"
fi

cmake_path="$(find_cmake_path)"
if [[ -z "$cmake_path" ]]; then
  printf 'cmake was not found. Install CMake or pass --qt-root/--qt6-dir for a Qt layout that includes it.\n' >&2
  exit 1
fi

if [[ -z "$cmake_path_from_env" ]]; then
  printf 'Warning: cmake was not found on PATH.\nUsing fallback CMake: %s\n' "$cmake_path" >&2
fi

build_dir="$(abs_path "$build_dir")"
release_dir="${build_dir}/release"

if [[ "$clean_build" = "ON" && -d "$build_dir" ]]; then
  rm -rf "$build_dir"
fi

benchmarks_value="ON"
if [[ "$no_benchmarks" = "ON" ]]; then
  benchmarks_value="OFF"
fi

configure_args=(
  -S "$project_root"
  -B "$build_dir"
  -D "CMAKE_BUILD_TYPE=${build_type}"
  -D "CHIRON_GUI_BUILD_BENCHMARKS=${benchmarks_value}"
  -D "CHIRON_GUI_ENABLE_QT_RUNTIME_DEPLOYMENT=${deploy_qt}"
  -D "CHIRON_GUI_OUTPUT_DIRECTORY=${release_dir}"
)

if [[ -n "$generator" ]]; then
  configure_args+=(-G "$generator")
fi

if [[ -n "$qt6_dir" ]]; then
  configure_args+=(-D "Qt6_DIR=${qt6_dir}")
elif [[ -n "$qt_root" ]]; then
  configure_args+=(-D "CMAKE_PREFIX_PATH=${qt_root}")
fi

run "$cmake_path" "${configure_args[@]}"
run "$cmake_path" --build "$build_dir" --target CloganGUI --config "$build_type" --parallel
cleanup_legacy_release_artifacts "$build_dir"

if [[ "$deploy_qt" = "ON" ]]; then
  invoke_qt_deployment "$build_dir" "$release_dir"
  assert_qt_deployment "$release_dir"
fi

printf '\nBuild complete: %s/CloganGUI\n' "$release_dir"
