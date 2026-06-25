{ pkgs, lib, config, inputs, ... }:

let
  qtPackages = [
    pkgs.qt6.qtbase
    pkgs.qt6.qtdeclarative
    pkgs.qt6.qttools
    pkgs.qt6.qtwayland
  ];

  qtQueryPrefix = pkgs.buildEnv {
    name = "image-viewport-qt-query-prefix";
    paths = qtPackages;
    pathsToLink = [
      "/bin"
      "/include"
      "/lib"
      "/libexec"
      "/mkspecs"
      "/share"
    ];
  };

  qmake6 = pkgs.lib.hiPrio (pkgs.writeShellScriptBin "qmake6" ''
    if [ "$1" = "-query" ]; then
      if [ "$#" -eq 1 ]; then
        "${pkgs.qt6.qtbase}/bin/qmake6" -query | while IFS= read -r line; do
          case "$line" in
            QT_INSTALL_PREFIX:*|QT_INSTALL_ARCHDATA:*|QT_INSTALL_DATA:*|QT_HOST_PREFIX:*|QT_HOST_DATA:*)
              printf '%s\n' "''${line%%:*}:${qtQueryPrefix}"
              ;;
            QT_INSTALL_HEADERS:*)
              printf '%s\n' "QT_INSTALL_HEADERS:${qtQueryPrefix}/include"
              ;;
            QT_INSTALL_LIBS:*|QT_HOST_LIBS:*)
              printf '%s\n' "''${line%%:*}:${qtQueryPrefix}/lib"
              ;;
            QT_INSTALL_LIBEXECS:*|QT_HOST_LIBEXECS:*)
              printf '%s\n' "''${line%%:*}:${qtQueryPrefix}/libexec"
              ;;
            QT_INSTALL_BINS:*|QT_HOST_BINS:*)
              printf '%s\n' "''${line%%:*}:${qtQueryPrefix}/bin"
              ;;
            QT_INSTALL_PLUGINS:*)
              printf '%s\n' "QT_INSTALL_PLUGINS:${qtQueryPrefix}/lib/qt-6/plugins"
              ;;
            QT_INSTALL_QML:*)
              printf '%s\n' "QT_INSTALL_QML:${qtQueryPrefix}/lib/qt-6/qml"
              ;;
            *)
              printf '%s\n' "$line"
              ;;
          esac
        done
        exit 0
      fi

      case "$2" in
        QT_HOST_BINS|QT_HOST_BINS/get|QT_INSTALL_BINS|QT_INSTALL_BINS/get)
          printf '%s\n' "${qtQueryPrefix}/bin"
          exit 0
          ;;
        QT_HOST_LIBEXECS|QT_HOST_LIBEXECS/get|QT_INSTALL_LIBEXECS|QT_INSTALL_LIBEXECS/get)
          printf '%s\n' "${qtQueryPrefix}/libexec"
          exit 0
          ;;
        QT_INSTALL_HEADERS)
          printf '%s\n' "${qtQueryPrefix}/include"
          exit 0
          ;;
        QT_INSTALL_LIBS)
          printf '%s\n' "${qtQueryPrefix}/lib"
          exit 0
          ;;
        QT_HOST_LIBS)
          printf '%s\n' "${qtQueryPrefix}/lib"
          exit 0
          ;;
        QT_INSTALL_PLUGINS)
          printf '%s\n' "${qtQueryPrefix}/lib/qt-6/plugins"
          exit 0
          ;;
        QT_INSTALL_QML)
          printf '%s\n' "${qtQueryPrefix}/lib/qt-6/qml"
          exit 0
          ;;
        QT_INSTALL_PREFIX|QT_INSTALL_ARCHDATA|QT_INSTALL_DATA|QT_HOST_PREFIX|QT_HOST_DATA)
          printf '%s\n' "${qtQueryPrefix}"
          exit 0
          ;;
      esac
    fi

    exec "${pkgs.qt6.qtbase}/bin/qmake6" "$@"
  '');

  qtBuildEnv = pkgs.buildEnv {
    name = "image-viewport-qt-build-env";
    paths = [
      qmake6
      qtQueryPrefix
    ];
    pathsToLink = [
      "/bin"
      "/include"
      "/lib"
      "/libexec"
      "/mkspecs"
      "/share"
    ];
  };
in
{
  env = {
    CMAKE_PREFIX_PATH = "${qtQueryPrefix}/lib/cmake";
    QML2_IMPORT_PATH = "${qtQueryPrefix}/lib/qt-6/qml";
    QT_PLUGIN_PATH = "${qtQueryPrefix}/lib/qt-6/plugins";
    Qt6_DIR = "${qtQueryPrefix}/lib/cmake/Qt6";
  };

  enterShell = ''
    unset QMAKE
  '';

  packages = [
    pkgs.cmake
    pkgs.git
    pkgs.just
    pkgs.libglvnd
    pkgs.ninja
    pkgs.pkg-config
    qtBuildEnv
  ];

  enterTest = ''
    just test
  '';
}
