# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
# Shared Qt/CMake tooling context consumed by devenv modules.
{
  pkgs,
  lib,
  kiriviewApp,
  karchivePackage ? pkgs.kdePackages.karchive,
}:
let
  inherit (kiriviewApp) appRoot repoRoot;
  qtToolPrefix = pkgs.symlinkJoin {
    name = "kiriview-qt-tools";
    paths = [
      karchivePackage
    ]
    ++ (with pkgs.kdePackages; [
      kconfig
      kimageformats
      ki18n
      kjobwidgets
      kirigami-addons
      kservice
      qtbase
      qtdeclarative
      qtimageformats
      qtmultimedia
      (qtmultimedia.dev or qtmultimedia)
      qtsvg
    ]);
  };
  qmake = pkgs.lib.hiPrio (
    pkgs.writeShellScriptBin "qmake6" ''
      if [ "$#" -eq 2 ] && [ "$1" = "-query" ]; then
        case "$2" in
          QT_INSTALL_PREFIX|QT_INSTALL_PREFIX/get|QT_HOST_PREFIX|QT_HOST_PREFIX/get|QT_INSTALL_ARCHDATA|QT_INSTALL_ARCHDATA/get|QT_INSTALL_DATA|QT_INSTALL_DATA/get|QT_HOST_DATA|QT_HOST_DATA/get)
            printf '%s\n' '${qtToolPrefix}'
            exit 0
            ;;
          QT_INSTALL_HEADERS|QT_INSTALL_HEADERS/get)
            printf '%s\n' '${qtToolPrefix}/include'
            exit 0
            ;;
          QT_INSTALL_LIBS|QT_INSTALL_LIBS/get|QT_HOST_LIBS|QT_HOST_LIBS/get)
            printf '%s\n' '${qtToolPrefix}/lib'
            exit 0
            ;;
          QT_HOST_LIBEXECS|QT_HOST_LIBEXECS/get|QT_INSTALL_LIBEXECS|QT_INSTALL_LIBEXECS/get)
            printf '%s\n' '${qtToolPrefix}/libexec'
            exit 0
            ;;
          QT_HOST_BINS|QT_HOST_BINS/get|QT_INSTALL_BINS|QT_INSTALL_BINS/get)
            printf '%s\n' '${qtToolPrefix}/bin'
            exit 0
            ;;
          QT_INSTALL_PLUGINS|QT_INSTALL_PLUGINS/get)
            printf '%s\n' '${qtToolPrefix}/lib/qt-6/plugins'
            exit 0
            ;;
          QT_INSTALL_QML|QT_INSTALL_QML/get)
            printf '%s\n' '${qtToolPrefix}/lib/qt-6/qml'
            exit 0
            ;;
        esac
      fi
      exec ${lib.getExe' pkgs.kdePackages.qtbase "qmake6"} "$@"
    ''
  );
  appQmlRoot = "${appRoot}/target/devenv/cmake";
  qmlImportPaths = [
    appQmlRoot
    "${pkgs.kdePackages.qtdeclarative}/lib/qt-6/qml"
    "${pkgs.kdePackages.qtmultimedia}/lib/qt-6/qml"
    "${pkgs.kdePackages.kconfig}/lib/qt-6/qml"
    "${pkgs.kdePackages.ki18n}/lib/qt-6/qml"
    "${pkgs.kdePackages.kirigami.unwrapped}/lib/qt-6/qml"
    "${pkgs.kdePackages.kirigami-addons}/lib/qt-6/qml"
  ];
  hostRuntimeLibraryPath = lib.concatStringsSep ":" [
    "${repoRoot}/.devenv/profile/lib"
    (lib.makeLibraryPath [
      (lib.getLib pkgs.pipewire)
      (lib.getLib pkgs.stdenv.cc.cc)
    ])
  ];
  qtPluginPath = "${repoRoot}/.devenv/profile/lib/qt-6/plugins";
  readSourceManifest =
    path:
    lib.filter (source: source != "" && !(lib.hasPrefix "#" source)) (
      lib.splitString "\n" (builtins.readFile path)
    );
  cppSources = lib.sort builtins.lessThan (
    (readSourceManifest ../../src/cpp_core_sources.txt)
    ++ (readSourceManifest ../../src/cpp_qml_sources.txt)
  );
  cppSourcesShellArgs = lib.escapeShellArgs cppSources;
  clazyIgnoreDirsRegex = "(^|/)(\\.devenv|target)(/|$)|^/nix/store/";
  qmlLintImportArgs = lib.escapeShellArgs (
    lib.concatMap (path: [
      "-I"
      path
    ]) qmlImportPaths
  );
  rustAnalyzerToml = # toml
    ''
      linkedProjects = ["Cargo.toml"]

      [cargo]
      targetDir = "target/rust-analyzer"

      [files]
      exclude = [
          ".cargo-vendor",
          ".devenv",
          ".direnv",
          ".flatpak-builder",
          ".flatpak-cargo",
          ".rumdl_cache",
          "build-dir",
          "repo",
          "target",
      ]
    '';
  refreshCompileCommands = pkgs.writeShellApplication {
    name = "kiriview-refresh-compile-commands";
    runtimeInputs = with pkgs; [
      cmake
      coreutils
      jq
    ];
    text = ''
      set -euo pipefail
      if (($# != 1)); then
          printf 'Usage: kiriview-refresh-compile-commands <jobs>\n' >&2
          exit 2
      fi
      jobs="$1"
      if ! [[ $jobs =~ ^[0-9]+$ ]] || ((jobs < 1)); then
          printf 'Invalid job count: %s\n' "$jobs" >&2
          exit 2
      fi

      app_root=${lib.escapeShellArg appRoot}
      cd "$app_root"
      build_dir="$app_root/target/devenv/cmake"
      output_db="$app_root/target/devenv/compile_commands.json"
      temporary_db="$output_db.tmp.$$"
      temporary_link="$app_root/compile_commands.json.tmp.$$"

      cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DKIRIVIEW_BUILD_TESTS=ON
      cmake --build "$build_dir" --target KiriViewCore --parallel "$jobs"
      jq \
          --arg app_root "$app_root" \
          'map(if (.file | startswith($app_root + "/")) then .file = (.file | .[($app_root | length) + 1:]) | .directory = $app_root else . end) | sort_by(.file, .directory)' \
          "$build_dir/compile_commands.json" >"$temporary_db"
      mv -f "$temporary_db" "$output_db"
      ln -sT "target/devenv/compile_commands.json" "$temporary_link"
      mv -Tf "$temporary_link" "$app_root/compile_commands.json"
    '';
  };
  cppLintPrelude = # sh
    ''
      set -euo pipefail
      cd ${lib.escapeShellArg appRoot}
      if [[ ! -f compile_commands.json ]]; then
          echo "compile_commands.json was not found; run 'devenv tasks run --mode single dev:app:lsp:refresh' to generate editor metadata" >&2
          exit 1
      fi
    '';
  qtBuildEnvironment = # sh
    ''
      export QMAKE=${lib.getExe' qmake "qmake6"}
    '';
  qtRuntimeEnvironment = # sh
    ''
      ${qtBuildEnvironment}
      export QT_PLUGIN_PATH=${lib.escapeShellArg qtPluginPath}
      export LD_LIBRARY_PATH=${lib.escapeShellArg hostRuntimeLibraryPath}"''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    '';
in
{
  inherit
    clazyIgnoreDirsRegex
    cppLintPrelude
    cppSourcesShellArgs
    qmake
    qmlLintImportArgs
    refreshCompileCommands
    rustAnalyzerToml
    qtBuildEnvironment
    qtRuntimeEnvironment
    ;

  enterShell = qtRuntimeEnvironment;

  qmllsGeneral = {
    buildDir = "${appQmlRoot}/org/hnjae/kiriview";
    importPaths = lib.concatStringsSep ":" qmlImportPaths;
    "no-cmake-calls" = true;
  };
}
