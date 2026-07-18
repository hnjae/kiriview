# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  config,
  lib,
  pkgs,
  ...
}:

{
  git-hooks.excludes = [ ".*\\.lock$" ];

  git-hooks.hooks = {
    detect-private-keys.enable = true;
    cocogitto = {
      enable = true;
      name = "cog verify";
      description = "Lint commit messages with Cocogitto.";
      package = pkgs.cocogitto;
      entry = ''
        ${lib.getExe pkgs.cocogitto} verify --file
      '';
      stages = [ "commit-msg" ];
    };
    reuse.enable = true;
    typos.enable = true;

    # Nix:
    deadnix.enable = true;
    statix.enable = true;

    # Markdown:
    rumdl.enable = true;

    # Shellcheck:
    shellcheck-env = {
      enable = true;
      name = "shellcheck";
      package = pkgs.shellcheck;
      files = ''
        (?x)^(
          .*\.(sh|bash)$|
          \.envrc(\..+)?$|
          \.env(\..+)?$
        )
      '';
      entry = lib.getExe pkgs.shellcheck;
    };
  };

  tasks."ci:git-hooks".exec = ''
    ${lib.getExe config.git-hooks.package} run --all-files
  '';
}
