# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ pkgs, ... }:
{
  packages = [
    pkgs.git
    pkgs.just
  ];

  languages.nix.enable = true;

  env.DEVENV_TUI = "false";
}
