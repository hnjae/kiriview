# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ pkgs, lib, ... }:
{
  packages = [
    pkgs.git
    pkgs.just

    # Git-hooks tools:
    pkgs.reuse
    pkgs.cocogitto
    pkgs.typos
    pkgs.deadnix
    pkgs.statix
    pkgs.rumdl
    pkgs.shellcheck

    # Treefmt tools:
    pkgs.rustfmt
    pkgs.taplo
    pkgs.yamlfmt
    pkgs.libxml2 # xmllint
    pkgs.biome
    pkgs.shellharden
    pkgs.shfmt
    (pkgs.runCommandLocal "qmlformat" { } ''
      mkdir -p "$out/bin"
      ln -s "${lib.getExe' pkgs.kdePackages.qtdeclarative "qmlformat"}" "$out/bin/qmlformat"
    '')

    # Native qt tools
    pkgs.clazy
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
    pkgs.cmake-format
    pkgs.clang-tools
  ];

  languages.nix.enable = true;

  env.DEVENV_TUI = "false";
}
