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
    status=0; \
    run-clang-tidy -p {{ build_dir }} -source-filter="$PWD/(src|tests|examples)/.*[.]cpp" -extra-arg=-I"$qt_headers" || status=$?; \
    clazy-standalone -p {{ build_dir }} --ignore-dirs="$qt_headers" --extra-arg=-I"$qt_headers" $(git ls-files '*.cpp' ':!tests/install_consumer/*.cpp') || status=$?; \
    cmake-lint {{ cmake_files }} || status=$?; \
    exit "$status"

clean:
    cmake -E rm -rf {{ build_dir }}
