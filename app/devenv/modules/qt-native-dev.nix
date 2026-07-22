# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  pkgs,
  lib,
  rustHost,
  kiriviewApp,
  ...
}:
let
  kiriviewLibHeif = pkgs.libheif.overrideAttrs (old: {
    cmakeFlags = (old.cmakeFlags or [ ]) ++ [
      "-DWITH_JPEG_DECODER=ON"
      "-DWITH_JPEG_DECODER_PLUGIN=OFF"
      "-DWITH_JPEG_ENCODER=ON"
      "-DWITH_JPEG_ENCODER_PLUGIN=OFF"
    ];
  });
  qtNative = import ../internal/qt-native-context.nix {
    inherit
      kiriviewApp
      lib
      pkgs
      ;
    karchivePackage = pkgs.kdePackages.karchive;
  };
in
{
  _module.args = {
    inherit qtNative;
  };

  enterShell = # sh
    ''
      ${qtNative.enterShell}
      ${rustHost.environment}
    '';

  files."app/rust-analyzer.toml".text = qtNative.rustAnalyzerToml;
  files."app/.qmlls.ini".ini.General = qtNative.qmllsGeneral;

  packages = [
    qtNative.qmake
    pkgs.kdePackages.karchive
    kiriviewLibHeif.bin
    kiriviewLibHeif.dev
    kiriviewLibHeif.lib
    pkgs.desktop-file-utils
    pkgs.flatpak-builder
    pkgs.jq
    pkgs.cargo-nextest
    pkgs.kdePackages.extra-cmake-modules
    pkgs.kdePackages.kconfig
    pkgs.kdePackages.kcoreaddons
    pkgs.kdePackages.kimageformats
    pkgs.kdePackages.ki18n
    pkgs.kdePackages.kjobwidgets
    pkgs.kdePackages.kirigami
    pkgs.kdePackages.kirigami-addons
    pkgs.kdePackages.kio
    pkgs.kdePackages.kservice
    pkgs.kdePackages.kio-extras
    pkgs.kdePackages.kio-fuse
    pkgs.kdePackages.qqc2-desktop-style
    pkgs.kdePackages.qtbase
    pkgs.kdePackages.qtdeclarative
    pkgs.kdePackages.qtimageformats
    pkgs.kdePackages.qtmultimedia
    pkgs.kdePackages.qtsvg
    pkgs.kdePackages.qttools
    pkgs.libarchive
    pkgs.libaom
    pkgs.libde265
    pkgs.libjxl.dev
    pkgs.libjxl.out
    pkgs.libraw.dev
    pkgs.libraw.lib
    pkgs.libwebp
    pkgs.pipewire
    pkgs.x265
  ];

  languages.rust.enable = true;
}
