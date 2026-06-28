#!/usr/bin/env -S just --justfile

set shell := ["sh", "-eu", "-c"]

build_dir := "build-ninja"

cmake_files := "$(git ls-files '*CMakeLists.txt' '*.cmake' '*.cmake.in')"

default:
    @just --list

configure:
    cmake -S . -B {{ build_dir }} -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
    cmake --build {{ build_dir }}

test: build
    ctest --test-dir {{ build_dir }} --output-on-failure

format:
    clang-format -i $(git ls-files '*.cpp' '*.h' '*.hpp')
    cmake-format -i {{ cmake_files }}

lint: configure
    qt_headers=$(qmake6 -query QT_INSTALL_HEADERS); \
    cpu_count=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1); \
    lint_jobs=${LINT_JOBS:-$((cpu_count / 2 + 1))}; \
    status=0; \
    run-clang-tidy -p {{ build_dir }} -j "$lint_jobs" -source-filter="$PWD/(src|tests|examples)/.*[.]cpp" -extra-arg=-I"$qt_headers" || status=$?; \
    git ls-files -z '*.cpp' ':!tests/install_consumer/*.cpp' | xargs -0 -n 1 -P "$lint_jobs" clazy-standalone -p {{ build_dir }} --header-filter="$PWD/src/.*" --ignore-dirs="$qt_headers" --extra-arg=-I"$qt_headers" --extra-arg=-Werror || status=$?; \
    cmake-lint {{ cmake_files }} || status=$?; \
    exit "$status"

clean:
    cmake -E rm -rf {{ build_dir }}
