# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
# Shared Qt/CMake tooling context consumed by devenv modules.
{
  pkgs,
  lib,
  kiriviewApp,
}:
let
  inherit (kiriviewApp) appRoot repoRoot;
  appQmlRoot = "${appRoot}/target/devenv/cmake";
  profileQmlRoot = "${repoRoot}/.devenv/profile/lib/qt-6/qml";
  kirigamiQmlRoot = "${pkgs.kdePackages.kirigami.unwrapped}/lib/qt-6/qml";
  qmlImportPaths = [
    appQmlRoot
    profileQmlRoot
    kirigamiQmlRoot
  ];
  hostRuntimeLibraryPath = lib.concatStringsSep ":" [
    "${repoRoot}/.devenv/profile/lib"
    (lib.makeLibraryPath [
      (lib.getLib pkgs.pipewire)
      (lib.getLib pkgs.stdenv.cc.cc)
    ])
  ];
  qtPluginPath = "${repoRoot}/.devenv/profile/lib/qt-6/plugins";
  clazyIgnoreDirsRegex = "(^|/)(\\.devenv|target)(/|$)|^/nix/store/";
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
  qtRuntimeEnvironment = # sh
    ''
      unset QMAKE
      export QT_PLUGIN_PATH=${lib.escapeShellArg qtPluginPath}
      export LD_LIBRARY_PATH=${lib.escapeShellArg hostRuntimeLibraryPath}"''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    '';
in
{
  inherit
    clazyIgnoreDirsRegex
    rustAnalyzerToml
    qtRuntimeEnvironment
    ;

  cmakeQmlImportPaths = lib.concatStringsSep ";" [ kirigamiQmlRoot ];
  enterShell = qtRuntimeEnvironment;

  qmllsGeneral = {
    buildDir = "${appQmlRoot}/org/hnjae/kiriview";
    importPaths = lib.concatStringsSep ":" qmlImportPaths;
    "no-cmake-calls" = true;
  };
}
