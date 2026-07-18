#!/usr/bin/env -S just --justfile

set unstable
set fallback := false
set lazy

_:
    @just --list

setup:
    devenv shell -- true

alias fmt := format

[group('ci')]
format:
    devenv shell -- treefmt

[group('ci')]
format-check:
    devenv shell -- treefmt --ci
