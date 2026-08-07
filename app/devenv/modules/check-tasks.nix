# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  pkgs,
  lib,
  qtNative,
  rustHost,
  kiriviewApp,
  ...
}:
let
  appRoot = kiriviewApp.appRoot;
  escapedAppRoot = lib.escapeShellArg appRoot;
  cmakeBuildDir = "target/devenv/cmake";
  escapedCmakeQmlImportPaths = lib.escapeShellArg qtNative.cmakeQmlImportPaths;
  baseTaskPrelude = # sh
    ''
      set -euo pipefail

      cd ${escapedAppRoot}

      export LC_ALL=C.UTF-8
      export LANG=C.UTF-8
    '';
  qtRuntimePrelude = # sh
    ''
      ${qtNative.qtRuntimeEnvironment}
      unset QT_ADDITIONAL_PACKAGES_PREFIX_PATH
    '';
  localJobsPrelude = # sh
    ''
      kiriview_default_local_jobs() {
          local cpu_count
          cpu_count="$(nproc)"
          printf '%d\n' "$((cpu_count / 2 + 1))"
      }
    '';
  jobsPrelude = # sh
    ''
      ${localJobsPrelude}

      if [[ -n ''${KIRIVIEW_JOBS:-} ]]; then
          kiriview_jobs="$KIRIVIEW_JOBS"
          kiriview_jobs_source="KIRIVIEW_JOBS"
      else
          kiriview_jobs="$(kiriview_default_local_jobs)"
          kiriview_jobs_source="local default job count"
      fi

      if ! [[ $kiriview_jobs =~ ^[0-9]+$ ]] || ((kiriview_jobs < 1)); then
          printf 'Invalid %s value: %s\n' "$kiriview_jobs_source" "$kiriview_jobs" >&2
          exit 2
      fi
    '';
  testJobsPrelude = # sh
    ''
      ${jobsPrelude}
      test_jobs="$kiriview_jobs"
    '';
  lintJobsPrelude = # sh
    ''
      ${jobsPrelude}
      lint_jobs="$kiriview_jobs"
    '';
  cargoJobsPrelude = # sh
    ''
      export CARGO_BUILD_JOBS="''${CARGO_BUILD_JOBS:-$kiriview_jobs}"
    '';
  cmakeConfigurePrelude = # sh
    ''
      unset QT_ADDITIONAL_PACKAGES_PREFIX_PATH
      cmake \
          -S . \
          -B ${cmakeBuildDir} \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_MAKE_PROGRAM=${lib.getExe pkgs.gnumake} \
          -DKIRIVIEW_BUILD_TESTS=ON \
          -DKIRIVIEW_QML_IMPORT_PATHS=${escapedCmakeQmlImportPaths}
    '';
  refreshCompileCommandsPrelude = # sh
    ''
      ${cmakeConfigurePrelude}
      cmake --build ${cmakeBuildDir} --target KiriViewCore --parallel "$kiriview_jobs"

      temporary_link="compile_commands.json.tmp.$$"
      ln -sT "${cmakeBuildDir}/compile_commands.json" "$temporary_link"
      mv -Tf "$temporary_link" compile_commands.json
    '';
  cppLintPrelude = # sh
    ''
      compile_db=${lib.escapeShellArg "${cmakeBuildDir}/compile_commands.json"}
      if [[ ! -f $compile_db ]]; then
          printf '%s was not found; run devenv tasks run --mode single ci:app:lint:cpp:prepare.\n' "$compile_db" >&2
          exit 1
      fi

      mapfile -d "" cpp_sources < <(
          ${lib.getExe pkgs.jq} \
              --raw-output0 \
              --arg source_root "$PWD/src/" \
              '
                  [
                      .[].file
                      | select(type == "string")
                      | select(startswith($source_root) and endswith(".cpp"))
                  ]
                  | unique[]
              ' \
              "$compile_db"
      )
      if ((''${#cpp_sources[@]} == 0)); then
          printf 'The compilation database does not contain any application C++ sources.\n' >&2
          exit 1
      fi
    '';
  cppLintTaskPrelude = # sh
    ''
      ${baseTaskPrelude}
      ${lintJobsPrelude}
      ${cppLintPrelude}
    '';
in
{
  tasks = {
    "dev:app:fix:cpp" = {
      description = "Apply clazy C++ fixits";
      showOutput = true;
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${lintJobsPrelude}
          ${cargoJobsPrelude}

          if [[ -z ''${CLAZY_FIXIT:-} ]]; then
              printf 'CLAZY_FIXIT is empty; no clazy fixits are configured.\n' >&2
              exit 2
          fi

          ${refreshCompileCommandsPrelude}
          ${cppLintPrelude}

          fixes_dir="$(mktemp -d)"
          trap 'rm -rf "$fixes_dir"' EXIT

          run-clazy-parallel \
              --jobs "$lint_jobs" \
              --clazy-binary ${lib.getExe' pkgs.clazy "clazy-standalone"} \
              --checks "$CLAZY_FIXIT" \
              --ignore-dirs=${lib.escapeShellArg qtNative.clazyIgnoreDirsRegex} \
              --export-fixes-dir "$fixes_dir" \
              -p ${cmakeBuildDir} \
              -- \
              "''${cpp_sources[@]}"

          ${lib.getExe' pkgs.clang-tools "clang-apply-replacements"} "$fixes_dir"
        '';
    };

    "dev:app:lsp:refresh" = {
      description = "Refresh the CMake compilation database for clangd";
      showOutput = true;
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${testJobsPrelude}
          ${cargoJobsPrelude}

          ${refreshCompileCommandsPrelude}
        '';
    };

    "ci:app:test:rust" = {
      description = "Run host Rust library tests";
      showOutput = true;
      before = [ "ci:test" ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${rustHost.environment}
          ${testJobsPrelude}

          printf 'Running host Rust tests with nextest using %d jobs...\n' "$test_jobs"
          cargo \
              nextest \
              run \
              --locked \
              --lib \
              --all-features \
              --build-jobs "$test_jobs" \
              --test-threads "$test_jobs"
        '';
    };

    "ci:app:test:cpp" = {
      description = "Run host C++ tests against the CMake-owned KiriView targets";
      showOutput = true;
      before = [ "ci:test" ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${qtRuntimePrelude}
          ${testJobsPrelude}
          ${cargoJobsPrelude}

          ${cmakeConfigurePrelude}
          printf 'Building and running host C++ tests with %d jobs...\n' "$test_jobs"
          cmake --build ${cmakeBuildDir} --parallel "$test_jobs"
          # GNU gettext ignores LANGUAGE under C/POSIX locales; devenv defaults to C.UTF-8.
          LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 \
              ctest \
                  --test-dir ${cmakeBuildDir} \
                  --tests-regex '^tst_' \
                  --output-on-failure \
                  --parallel "$test_jobs"
        '';
    };

    "ci:app:lint:rust" = {
      description = "Run Rust clippy";
      showOutput = true;
      before = [ "ci:lint" ];
      after = [
        "ci:app:test:rust@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${rustHost.environment}
          ${lintJobsPrelude}

          cargo \
              clippy \
              --locked \
              --all-targets \
              --all-features \
              --jobs "$lint_jobs" \
              -- \
              -D warnings \
              2>&1 \
              | cat
        '';
    };

    "ci:app:lint:qml" = {
      description = "Run qmllint against QML sources";
      showOutput = true;
      before = [ "ci:lint" ];
      after = [
        "ci:app:test:cpp@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${lintJobsPrelude}
          ${cargoJobsPrelude}

          unset LD_LIBRARY_PATH
          unset QT_PLUGIN_PATH
          unset QT_ADDITIONAL_PACKAGES_PREFIX_PATH

          ${cmakeConfigurePrelude}
          cmake --build ${cmakeBuildDir} --target KiriViewQml_qmllint --parallel "$lint_jobs" \
              2>&1 \
              | sed \
                  -e '/^Two plugins named "Quick" present, make sure no plugins are duplicated\. The second plugin will not be loaded\.$/d' \
                  -e '/^Two plugins named "QtDesignStudio" present, make sure no plugins are duplicated\. The second plugin will not be loaded\.$/d'
        '';
    };

    "ci:app:lint:flatpak" = {
      description = "Check Flatpak manifest permission policy";
      showOutput = true;
      before = [ "ci:lint" ];
      exec = # sh
        ''
          ${baseTaskPrelude}

          manifest="org.hnjae.kiriview.json"

          has_finish_arg() {
              ${lib.getExe pkgs.jq} \
                  --exit-status \
                  --arg arg "$1" \
                  '.["finish-args"] | arrays | index($arg) != null' \
                  "$manifest" >/dev/null
          }

          require_arg() {
              if ! has_finish_arg "$1"; then
                  printf 'Flatpak manifest must include permission: %s\n' "$1" >&2
                  return 1
              fi
          }

          forbid_arg() {
              if has_finish_arg "$1"; then
                  printf 'Flatpak manifest must not include broad permission: %s\n' "$1" >&2
                  return 1
              fi
          }

          require_arg "--nofilesystem=/run/user"
          require_arg "--filesystem=xdg-run/pipewire-0"
          require_arg "--filesystem=xdg-run/gvfs"
          require_arg "--filesystem=xdg-cache/thumbnails:create"
          require_arg "--talk-name=org.kde.KIOFuse"

          forbid_arg "--filesystem=/run/user"
          forbid_arg "--filesystem=/run/user:ro"
          forbid_arg "--filesystem=xdg-run"
          forbid_arg "--filesystem=xdg-cache"
          forbid_arg "--filesystem=xdg-cache:ro"
          forbid_arg "--filesystem=xdg-cache:create"
        '';
    };

    "ci:app:lint:desktop" = {
      description = "Validate desktop metadata";
      showOutput = true;
      before = [ "ci:lint" ];
      exec = # sh
        ''
          ${baseTaskPrelude}

          ${lib.getExe' pkgs.desktop-file-utils "desktop-file-validate"} org.hnjae.kiriview.desktop
        '';
    };

    "ci:app:lint:identity" = {
      description = "Validate installed application identity artifacts";
      showOutput = true;
      before = [ "ci:lint" ];
      exec = # sh
        ''
          ${baseTaskPrelude}

          ${lib.getExe pkgs.bash} scripts/check-application-identity.sh
        '';
    };

    "ci:app:lint:cpp:prepare" = {
      description = "Prepare the app C++ compilation database for linting";
      showOutput = true;
      after = [
        "ci:app:lint:qml@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${lintJobsPrelude}
          ${cargoJobsPrelude}

          ${refreshCompileCommandsPrelude}
          ${cppLintPrelude}
        '';
    };

    "ci:app:lint:cpp:clang-tidy" = {
      description = "Run clang-tidy against app C++ sources";
      showOutput = true;
      after = [
        "ci:app:lint:cpp:prepare@succeeded"
      ];
      exec = # sh
        ''
          ${cppLintTaskPrelude}

          ${lib.getExe' pkgs.llvmPackages.clang-unwrapped "run-clang-tidy"} \
              -clang-tidy-binary ${lib.getExe' pkgs.clang-tools "clang-tidy"} \
              -header-filter=${lib.escapeShellArg "^${appRoot}/src/"} \
              -exclude-header-filter=${lib.escapeShellArg "^${appRoot}/(target|build-dir)/"} \
              -p ${cmakeBuildDir} \
              -j "$lint_jobs" \
              -quiet \
              "''${cpp_sources[@]}"
        '';
    };

    "ci:app:lint:cpp:clazy" = {
      description = "Run clazy against app C++ sources";
      showOutput = true;
      before = [ "ci:lint" ];
      after = [
        "ci:app:lint:cpp:clang-tidy@succeeded"
      ];
      exec = # sh
        ''
          ${cppLintTaskPrelude}

          run-clazy-parallel \
              --jobs "$lint_jobs" \
              --clazy-binary ${lib.getExe' pkgs.clazy "clazy-standalone"} \
              --checks "''${CLAZY_CHECKS:-level0}" \
              --ignore-dirs=${lib.escapeShellArg qtNative.clazyIgnoreDirsRegex} \
              -p ${cmakeBuildDir} \
              -- \
              "''${cpp_sources[@]}"
        '';
    };
  };
}
