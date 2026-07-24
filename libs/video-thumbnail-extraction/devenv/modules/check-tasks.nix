# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  kiriviewVideoThumbnailExtraction,
  lib,
  pkgs,
  ...
}:
let
  componentRoot = lib.escapeShellArg kiriviewVideoThumbnailExtraction.root;
  qtHeaders = lib.escapeShellArg "${lib.getDev pkgs.kdePackages.qtbase}/include";
  buildDir = "build-ninja";
  taskPrelude = # sh
    ''
      set -euo pipefail
      cd ${componentRoot}
    '';
  configurePrelude = # sh
    ''
      cmake \
          -S . \
          -B ${buildDir} \
          -G Ninja \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    '';
  lintJobsPrelude = # sh
    ''
      cpu_count="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
      lint_jobs="''${LINT_JOBS:-$((cpu_count / 2 + 1))}"
    '';
in
{
  tasks = {
    "ci:video-thumbnail-extraction:test" = {
      description = "Build VideoThumbnailExtraction";
      showOutput = true;
      before = [ "ci:test" ];
      exec = # sh
        ''
          ${taskPrelude}
          ${configurePrelude}

          cmake --build ${buildDir}
        '';
    };

    "ci:video-thumbnail-extraction:lint:prepare" = {
      description = "Prepare the VideoThumbnailExtraction compilation database";
      showOutput = true;
      after = [
        "ci:video-thumbnail-extraction:test@succeeded"
      ];
      exec = # sh
        ''
          ${taskPrelude}
          ${configurePrelude}
        '';
    };

    "ci:video-thumbnail-extraction:lint:clang-tidy" = {
      description = "Run clang-tidy against VideoThumbnailExtraction C++ sources";
      showOutput = true;
      after = [
        "ci:video-thumbnail-extraction:lint:prepare@succeeded"
      ];
      exec = # sh
        ''
          ${taskPrelude}
          ${lintJobsPrelude}

          run-clang-tidy \
              -p ${buildDir} \
              -j "$lint_jobs" \
              -quiet \
              -source-filter="$PWD/src/.*[.]cpp" \
              -header-filter="$PWD/src/.*"
        '';
    };

    "ci:video-thumbnail-extraction:lint:clazy" = {
      description = "Run clazy against VideoThumbnailExtraction C++ sources";
      showOutput = true;
      before = [ "ci:lint" ];
      after = [
        "ci:video-thumbnail-extraction:lint:clang-tidy"
      ];
      exec = # sh
        ''
          ${taskPrelude}
          ${lintJobsPrelude}

          mapfile -d "" clazy_sources < <(
              ${lib.getExe pkgs.jq} \
                  --raw-output0 \
                  --arg build_dir ${buildDir} \
                  --arg source_root "$PWD" \
                  '
                      [
                          .[].file
                          | select(type == "string")
                          | if startswith($source_root + "/") then
                              .[($source_root | length) + 1:]
                            elif startswith("/") then
                              empty
                            else
                              .
                            end
                          | select(startswith($build_dir + "/") | not)
                          | select(endswith(".cpp"))
                      ]
                      | unique[]
                  ' \
                  ${buildDir}/compile_commands.json
          )
          if ((''${#clazy_sources[@]} == 0)); then
              printf 'The compilation database does not contain any repository-authored C++ sources.\n' >&2
              exit 1
          fi

          run-clazy-parallel \
              --jobs "$lint_jobs" \
              --clazy-binary ${lib.getExe' pkgs.clazy "clazy-standalone"} \
              --checks "''${CLAZY_CHECKS:-level0}" \
              --header-filter="$PWD/src/.*" \
              --ignore-dirs=${qtHeaders} \
              -p ${buildDir} \
              -- \
              "''${clazy_sources[@]}"
        '';
    };

  };
}
