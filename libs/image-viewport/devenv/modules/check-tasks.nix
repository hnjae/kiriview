# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  kiriviewImageViewport,
  lib,
  pkgs,
  ...
}:
let
  imageViewportRoot = lib.escapeShellArg kiriviewImageViewport.root;
  qtHeaders = lib.escapeShellArg "${lib.getDev pkgs.kdePackages.qtbase}/include";
  buildDir = "build-ninja";
  taskPrelude = # sh
    ''
      set -euo pipefail
      cd ${imageViewportRoot}
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
    "ci:image-viewport:test" = {
      description = "Build and test ImageViewport";
      showOutput = true;
      before = [ "ci:test" ];
      exec = # sh
        ''
          ${taskPrelude}
          ${configurePrelude}

          cmake --build ${buildDir}
          ctest --test-dir ${buildDir} --output-on-failure
        '';
    };

    "ci:image-viewport:lint:prepare" = {
      description = "Prepare the ImageViewport compilation database for linting";
      showOutput = true;
      after = [
        "ci:image-viewport:test@succeeded"
      ];
      exec = # sh
        ''
          ${taskPrelude}
          ${configurePrelude}
        '';
    };

    "ci:image-viewport:lint:clang-tidy" = {
      description = "Run clang-tidy against ImageViewport C++ sources";
      showOutput = true;
      after = [
        "ci:image-viewport:lint:prepare@succeeded"
      ];
      exec = # sh
        ''
          ${taskPrelude}
          ${lintJobsPrelude}

          run-clang-tidy \
              -p ${buildDir} \
              -j "$lint_jobs" \
              -quiet \
              -source-filter="$PWD/(src|tests|examples)/.*[.]cpp" \
              -header-filter="$PWD/(src|tests|examples)/.*"
        '';
    };

    "ci:image-viewport:lint:clazy" = {
      description = "Run clazy against ImageViewport C++ sources";
      showOutput = true;
      after = [
        "ci:image-viewport:lint:clang-tidy"
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

    "ci:image-viewport:lint:cmake" = {
      description = "Lint ImageViewport CMake sources";
      showOutput = true;
      before = [ "ci:lint" ];
      after = [
        "ci:image-viewport:lint:clazy"
      ];
      exec = # sh
        ''
          ${taskPrelude}

          mapfile -d "" cmake_files < <(
              git ls-files -z '*CMakeLists.txt' '*.cmake' '*.cmake.in'
          )
          cmake-lint "''${cmake_files[@]}"
        '';
    };
  };
}
