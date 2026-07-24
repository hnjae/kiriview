#!/usr/bin/env -S just --justfile

set unstable
set fallback := false
set lazy

_:
    @just --list

alias fmt := format

[group('ci')]
format:
    devenv shell -- treefmt

[group('ci')]
format-check:
    devenv shell -- treefmt --ci

[group('ci')]
lint:
    devenv shell -- devenv tasks run ci:lint

[group('ci')]
test:
    devenv shell -- devenv tasks run ci:test

alias check := ci

[group('ci')]
ci:
    devenv shell -- devenv tasks run --mode=single ci

refresh-fc-cache:
    flatpak run --user --command=fc-cache org.hnjae.kiriview -rv /run/host/fonts

[group('build')]
run:
    #!/bin/sh
    set -eu

    runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

    set -- \
        --socket=wayland \
        --socket=pulseaudio \
        --device=dri \
        --talk-name=org.freedesktop.portal.Desktop \
        --talk-name=org.kde.KIOFuse \
        --filesystem=home \
        --filesystem=/media \
        --filesystem=/mnt \
        --filesystem=/run/media \
        --nofilesystem=/run/user \
        --filesystem=xdg-run/pipewire-0 \
        --filesystem=xdg-run/gvfs \
        --env=LIBHEIF_PLUGIN_PATH=/app/lib/libheif \
        --env=QSG_RHI_BACKEND="${QSG_RHI_BACKEND:-vulkan}" \
        --env=WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}" \
        --env=QT_QPA_PLATFORM=wayland \
        --env=QT_LOGGING_RULES="${QT_LOGGING_RULES:-org.hnjae.kiriview.input.debug=true;org.hnjae.kiriview.predecode.debug=true;org.hnjae.kiriview.decode.debug=true}"

    if [ -d "$runtime_dir/doc" ]; then
        set -- "$@" --bind-mount="$runtime_dir/doc=$runtime_dir/doc"
    fi

    for kio_fuse_path in "$runtime_dir"/kio-fuse-*; do
        [ -d "$kio_fuse_path" ] || continue
        set -- "$@" "--filesystem=xdg-run/$(basename "$kio_fuse_path")"
    done

    flatpak build \
        "$@" \
        app/build-dir \
        kiriview

[group('build')]
install:
    flatpak-builder \
        --install \
        --user \
        --disable-tests \
        --install-deps-from=flathub \
        --default-branch main \
        --delete-build-dirs \
        --force-clean app/build-dir \
        app/org.hnjae.kiriview.json

[group('build')]
build:
    flatpak-builder \
        --disable-tests \
        --install-deps-from=flathub \
        --delete-build-dirs \
        --force-clean app/build-dir \
        app/org.hnjae.kiriview.json

[group('build')]
build-flatpak-with-test:
    flatpak-builder \
        --install-deps-from=flathub \
        --delete-build-dirs \
        --force-clean app/build-dir \
        app/org.hnjae.kiriview.json
