{ pkgs, lib, config, inputs, ... }:

let
  qtPackages = [
    pkgs.qt6.qtbase
    pkgs.qt6.qtdeclarative
  ];
in
{
  env = {
    CMAKE_PREFIX_PATH = lib.makeSearchPath "lib/cmake" qtPackages;
    QML2_IMPORT_PATH = lib.makeSearchPath "lib/qt-6/qml" qtPackages;
    QT_PLUGIN_PATH = lib.makeSearchPath "lib/qt-6/plugins" qtPackages;
    Qt6_DIR = "${pkgs.qt6.qtbase}/lib/cmake/Qt6";
  };

  packages = [
    pkgs.cmake
    pkgs.git
    pkgs.just
    pkgs.ninja
    pkgs.pkg-config
  ] ++ qtPackages;

  enterTest = ''
    just test
  '';
}
