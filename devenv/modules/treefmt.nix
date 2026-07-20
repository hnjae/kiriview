# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  config,
  pkgs,
  lib,
  ...
}:
{
  packages = [
    pkgs.rustfmt
    pkgs.taplo
    pkgs.yamlfmt
    pkgs.libxml2
    pkgs.biome
    pkgs.shellharden
    pkgs.shfmt
    (pkgs.runCommandLocal "qmlformat" { } ''
      mkdir -p "$out/bin"
      ln -s "${lib.getExe' pkgs.kdePackages.qtdeclarative "qmlformat"}" "$out/bin/qmlformat"
    '')
  ];

  treefmt = {
    enable = true;

    config = {
      settings.excludes = [ "*.lock" ];
      programs = {
        cmake-format = {
          enable = true;
          includes = [
            "*.cmake"
            "*.cmake.in"
            "CMakeLists.txt"
            "*/CMakeLists.txt"
          ];
        };
        just.enable = true;
        nixfmt.enable = true;
        rustfmt = {
          enable = true;
          package = pkgs.rustfmt;
        };
        taplo.enable = true;
        yamlfmt = {
          enable = true;
          includes = [
            "*.yaml"
            "*.yml"
            ".clang-format"
            "*/.clang-format"
            ".clang-tidy"
            "*/.clang-tidy"
          ];
        };
        xmllint = {
          enable = true;
          includes = [
            "*.xml"
            "*.svg"
            "*.kcfg"
          ];
        };
      };

      settings.formatter = {
        biome = {
          command = lib.getExe pkgs.biome;
          options = [
            "format"
            "--write"
            "--no-errors-on-unmatched"
          ];
          includes = [
            "*.js"
            "*.ts"
            "*.mjs"
            "*.mts"
            "*.cjs"
            "*.cts"
            "*.jsx"
            "*.tsx"
            "*.d.ts"
            "*.d.cts"
            "*.d.mts"
            "*.json"
            "*.jsonc"
            "*.css"
          ];
        };

        clang-format = {
          command = lib.getExe' pkgs.clang-tools "clang-format";
          options = [
            "-i"
            "--style=file"
          ];
          includes = [
            "*.c"
            "*.cc"
            "*.cpp"
            "*.cxx"
            "*.h"
            "*.hh"
            "*.hpp"
            "*.hxx"
          ];
        };

        qmlformat = {
          command = lib.getExe' (pkgs.runCommandLocal "qmlformat" { } ''
            mkdir -p "$out/bin"
            ln -s "${lib.getExe' pkgs.kdePackages.qtdeclarative "qmlformat"}" "$out/bin/qmlformat"
          '') "qmlformat";
          options = [ "-i" ];
          includes = [ "*.qml" ];
        };

        rumdl = {
          command = lib.getExe pkgs.rumdl;
          options = [ "fmt" ];
          includes = [ "*.md" ];
        };

        shell-format = {
          command = lib.getExe (
            pkgs.writeShellApplication {
              name = "treefmt-shell-format";
              runtimeInputs = with pkgs; [
                shellharden
                shfmt
              ];
              text = ''
                for file in "$@"; do
                  shellharden --replace "$file"
                  shfmt --indent 4 --simplify --write "$file"
                done
              '';
            }
          );
          includes = [
            "*.sh"
            "*.bash"
            ".envrc"
            ".envrc.*"
            ".env"
            ".env.*"
          ];
        };
      };
    };
  };

  files."treefmt.toml".source = config.treefmt.config.build.configFile;

  tasks."ci:repo:format" = {
    description = "Check repository formatting";
    before = [ "ci:lint" ];
    exec = ''
      ${lib.getExe config.treefmt.config.build.wrapper} --ci
    '';
  };

  # Keep treefmt available without formatting the repository on every devenv shell entry.
  tasks."devenv:treefmt:run".before = lib.mkForce [ ];
}
