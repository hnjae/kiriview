# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  imports = [
    ./workspace-tools.nix
    ./native-tooling.nix
    ./clazy-runner.nix
    ./cpp-lint-policy.nix
    ./treefmt.nix
    ./git-hooks.nix
    ./ci.nix
  ];
}
