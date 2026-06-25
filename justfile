#!/usr/bin/env -S just --justfile

set shell := ["sh", "-eu", "-c"]

build_dir := "build"

default:
    @just --list

configure:
    cmake -S . -B {{ build_dir }} -G Ninja

build: configure
    cmake --build {{ build_dir }}

test: build
    ctest --test-dir {{ build_dir }} --output-on-failure

clean:
    cmake -E rm -rf {{ build_dir }}
