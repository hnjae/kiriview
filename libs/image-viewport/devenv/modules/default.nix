# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ pkgs, ... }:
{
  imports = [
    ./context.nix
    ./check-tasks.nix
  ];

  packages = [
    pkgs.kdePackages.qtbase
    pkgs.kdePackages.qtdeclarative
  ];
}
