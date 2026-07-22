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
  qtBuildPrelude = # sh
    ''
      ${qtNative.qtBuildEnvironment}
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
  cppLintTaskPrelude = # sh
    ''
      ${baseTaskPrelude}
      ${qtBuildPrelude}
      ${rustHost.environment}
      ${lintJobsPrelude}
      ${qtNative.cppLintPrelude}
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
          ${qtBuildPrelude}
          ${rustHost.environment}
          ${lintJobsPrelude}

          if [[ -z ''${CLAZY_FIXIT:-} ]]; then
              printf 'CLAZY_FIXIT is empty; no clazy fixits are configured.\n' >&2
              exit 2
          fi

          ${lib.getExe qtNative.refreshCompileCommands} "$lint_jobs"
          ${qtNative.cppLintPrelude}

          fixes_dir="$(mktemp -d)"
          trap 'rm -rf "$fixes_dir"' EXIT

          run-clazy-parallel \
              --jobs "$lint_jobs" \
              --clazy-binary ${lib.getExe' pkgs.clazy "clazy-standalone"} \
              --checks "$CLAZY_FIXIT" \
              --ignore-dirs=${lib.escapeShellArg qtNative.clazyIgnoreDirsRegex} \
              --export-fixes-dir "$fixes_dir" \
              -p . \
              -- \
              ${qtNative.cppSourcesShellArgs}

          ${lib.getExe' pkgs.clang-tools "clang-apply-replacements"} "$fixes_dir"
        '';
    };

    "dev:app:lsp:refresh" = {
      description = "Refresh generated LSP metadata for rust-analyzer and clangd";
      showOutput = true;
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${qtBuildPrelude}
          ${rustHost.environment}
          ${testJobsPrelude}

          ${lib.getExe qtNative.refreshCompileCommands} "$test_jobs"
        '';
    };

    "ci:app:test:rust" = {
      description = "Run host Rust library and doc tests";
      showOutput = true;
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${qtRuntimePrelude}
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

          printf 'Running host Rust doc tests with %d jobs...\n' "$test_jobs"
          cargo \
              test \
              --doc \
              --locked \
              --all-features \
              --jobs "$test_jobs" \
              -- \
              --test-threads "$test_jobs"
        '';
    };

    "ci:app:test:cpp" = {
      description = "Run host C++ tests against the CMake-owned KiriView targets";
      showOutput = true;
      before = [ "ci:test" ];
      after = [
        "ci:app:test:rust@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${qtRuntimePrelude}
          ${rustHost.environment}
          ${testJobsPrelude}

          cmake \
              -S . \
              -B target/devenv/cmake \
              -DCMAKE_BUILD_TYPE=Debug \
              -DKIRIVIEW_BUILD_TESTS=ON
          printf 'Building and running host C++ tests with %d jobs...\n' "$test_jobs"
          cmake --build target/devenv/cmake --parallel "$test_jobs"
          # GNU gettext ignores LANGUAGE under C/POSIX locales; devenv defaults to C.UTF-8.
          LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 \
              ctest \
                  --test-dir target/devenv/cmake \
                  --tests-regex '^tst_' \
                  --output-on-failure \
                  --parallel "$test_jobs"
        '';
    };

    "ci:app:lint:rust" = {
      description = "Run Rust clippy";
      showOutput = true;
      after = [
        "ci:app:test:cpp@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${qtBuildPrelude}
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
          ${qtBuildPrelude}
          ${rustHost.environment}
          ${lintJobsPrelude}

          cmake \
              -S . \
              -B target/devenv/cmake \
              -DCMAKE_BUILD_TYPE=Debug \
              -DKIRIVIEW_BUILD_TESTS=ON
          cmake --build target/devenv/cmake --target KiriViewQml --parallel "$lint_jobs"

          unset LD_LIBRARY_PATH
          unset QT_PLUGIN_PATH
          unset QT_ADDITIONAL_PACKAGES_PREFIX_PATH

          ${lib.getExe' pkgs.kdePackages.qtdeclarative "qmllint"} ${qtNative.qmlLintImportArgs} --ignore-settings --max-warnings 0 src/qml/*.qml \
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

    "ci:app:lint:repo-policy" = {
      description = "Check repository artifact and build-shape policy";
      showOutput = true;
      before = [ "ci:lint" ];
      exec = # sh
        ''
          ${baseTaskPrelude}

          violations=()

          report_violation() {
              violations+=("$1")
          }

          require_file_absent() {
              local path="$1"
              if [[ -e $path ]]; then
                  report_violation "$path must not exist"
              fi
          }

          forbid_token_in_file() {
              local file="$1"
              local token="$2"
              if grep -Fq -- "$token" "$file"; then
                  report_violation "$file must not contain $token"
              fi
          }

          require_token_in_file() {
              local file="$1"
              local token="$2"
              if ! grep -Fq -- "$token" "$file"; then
                  report_violation "$file must contain $token"
              fi
          }

          forbidden_artifact_files=(
              src/facade/kiriimageview.cpp
              src/facade/kiriimageview.h
              src/rendering/decodedtilecache.cpp
              src/rendering/decodedtilecache.h
              src/rendering/displayedimagesurfacestate.cpp
              src/rendering/displayedimagesurfacestate.h
              src/rendering/imagerenderframe.cpp
              src/rendering/imagerenderframe.h
              src/rendering/imagerendernodestate.cpp
              src/rendering/imagerendernodestate.h
              src/rendering/imagetiledecoderuntime.cpp
              src/rendering/imagetiledecoderuntime.h
              src/rendering/imagetiledecodescheduler.cpp
              src/rendering/imagetiledecodescheduler.h
              src/rendering/imagetiledecodestate.cpp
              src/rendering/imagetiledecodestate.h
              src/rendering/imagetilerequestplan.cpp
              src/rendering/imagetilerequestplan.h
              src/rendering/imagesurface.cpp
              src/rendering/imagesurface.h
              src/rendering/kiriimagerendernode.cpp
              src/rendering/kiriimagerendernode.h
              src/shaders/kiriimageview.frag
              src/shaders/kiriimageview.vert
              src/shaders/kiriimageview_shaders.h
              src/facade/imageshortcutnavigationpolicy.h
              src/facade/imageshortcutnavigationpolicy.cpp
          )
          source_manifests=(
              src/cpp_core_sources.txt
              src/cpp_qml_header_sources.txt
              src/cpp_qml_sources.txt
          )

          for path in "''${forbidden_artifact_files[@]}"; do
              require_file_absent "$path"
              for manifest in "''${source_manifests[@]}"; do
                  forbid_token_in_file "$manifest" "$path"
              done
          done

          for token in \
              QT_RHI \
              add_qt_rhi_include_dirs \
              bake_shaders \
              run_qsb \
              qsb \
              qshader \
              kiriimageview.vert \
              kiriimageview.frag \
              kiriimageview_shaders; do
              forbid_token_in_file build.rs "$token"
          done

          for token in \
              kiriview_qt_rhi_include_dirs \
              KIRIVIEW_QT_RHI_INCLUDE_DIRS \
              rhi/qrhi.h \
              rhi/qshader.h \
              kiriview_manifest_sources \
              cpp_core_sources.txt \
              cpp_qml_header_sources.txt \
              cpp_qml_sources.txt \
              KIRIVIEW_CORE_SOURCE_PATHS \
              kconfig_add_kcfg_files; do
              forbid_token_in_file tests/cpp/CMakeLists.txt "$token"
          done

          for token in \
              KiriViewCargoStatic \
              'add_library(kiriview_test_core INTERFACE' \
              LINK_LIBRARY:WHOLE_ARCHIVE; do
              forbid_token_in_file tests/cpp/CMakeLists.txt "$token"
          done

          if ((''${#violations[@]} > 0)); then
              printf '%s\n' "''${violations[@]}" >&2
              exit 1
          fi
        '';
    };

    "ci:app:lint:compile-db" = {
      description = "Check compile_commands.json shape";
      showOutput = true;
      before = [ "ci:lint" ];
      after = [
        "ci:app:lint:cpp:clazy@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}

          compile_db="compile_commands.json"
          generated_compile_db="target/devenv/compile_commands.json"

          if [[ ! -L $compile_db ]]; then
              printf '%s must be a symlink generated by dev:app:lsp:refresh.\n' "$compile_db" >&2
              exit 1
          fi

          resolved_compile_db="$(readlink -f "$compile_db")"
          expected_compile_db="$(readlink -m "$PWD/$generated_compile_db")"
          if [[ $resolved_compile_db != "$expected_compile_db" ]]; then
              printf '%s must resolve to %s; got %s.\n' "$compile_db" "$expected_compile_db" "$resolved_compile_db" >&2
              exit 1
          fi

          ${lib.getExe pkgs.jq} --exit-status '
              type == "array"
              and any(.[]; (.file | strings | (startswith("src/") and endswith(".cpp"))))
              and any(.[]; (.file | strings | (startswith("tests/cpp/tst_") and endswith(".cpp"))))
          ' "$compile_db" >/dev/null
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

    "ci:app:lint:cpp:prepare" = {
      description = "Prepare the app C++ compilation database for linting";
      showOutput = true;
      after = [
        "ci:app:lint:rust@succeeded"
      ];
      exec = # sh
        ''
          ${baseTaskPrelude}
          ${qtBuildPrelude}
          ${rustHost.environment}
          ${lintJobsPrelude}

          ${lib.getExe qtNative.refreshCompileCommands} "$lint_jobs"
          ${qtNative.cppLintPrelude}
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
              -p . \
              -j "$lint_jobs" \
              -quiet \
              ${qtNative.cppSourcesShellArgs}
        '';
    };

    "ci:app:lint:cpp:clazy" = {
      description = "Run clazy against app C++ sources";
      showOutput = true;
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
              -p . \
              -- \
              ${qtNative.cppSourcesShellArgs}
        '';
    };
  };
}
