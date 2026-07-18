# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

{ pkgs, ... }:
let
  qtQueryPrefix = pkgs.buildEnv {
    name = "qt-query-prefix";
    paths = [
      pkgs.qt6.qtbase
      pkgs.qt6.qtdeclarative
      pkgs.qt6.qttools
      pkgs.qt6.qtwayland
    ];
    pathsToLink = [
      "/bin"
      "/include"
      "/lib"
      "/libexec"
      "/metatypes"
      "/mkspecs"
      "/share"
    ];
  };

  qmake6 = pkgs.lib.hiPrio (
    pkgs.writeShellScriptBin "qmake6" ''
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
    ''
  );

in
{
  stdenv = pkgs.clangStdenv;

  env = {
    CMAKE_PREFIX_PATH = "${qtQueryPrefix}/lib/cmake";
    CPLUS_INCLUDE_PATH =
      let
        gccCxxInclude = "${pkgs.gcc.cc}/include/c++/${pkgs.gcc.version}";
        gccTargetCxxInclude = "${gccCxxInclude}/${pkgs.stdenv.hostPlatform.config}";
      in
      "${gccCxxInclude}:${gccTargetCxxInclude}:${pkgs.glibc.dev}/include";
    QML2_IMPORT_PATH = "${qtQueryPrefix}/lib/qt-6/qml";
    QT_PLUGIN_PATH = "${qtQueryPrefix}/lib/qt-6/plugins";
    Qt6_DIR = "${qtQueryPrefix}/lib/cmake/Qt6";
    CLAZY_CHECKS = builtins.concatStringsSep "," [
      # Checks from Manual Level:
      # "assert-with-side-effects" # managed by clang-tidy: bugprone-assert-side-effect
      # "unneeded-cast"            # managed by clang-tidy: readability-redundant-casting/cppcoreguidelines-pro-type-*
      # "unused-result-check"      # managed by clang-tidy: bugprone-unused-return-value
      "ifndef-define-typo"
      "qhash-with-char-pointer-key"
      "qproperty-type-mismatch"
      "qstring-varargs"
      "signal-with-return-value"
      "unexpected-flag-enumerator-value"
      "thread-with-slots"
      "tr-non-literal"
      "raw-environment-function"
      "qvariant-template-instantiation"
      "container-inside-loop"
      "detaching-member"
      "isempty-vs-count"
      "reserve-candidates"
      "use-chrono-in-qtimer"
      "use-arrow-operator-instead-of-data"

      # Checks from Level 0-1:
      "level1"
      "no-rule-of-two-soft" # managed by clang-tidy: cppcoreguidelines-special-member-functions

      # Checks from Level 2:
      # "function-args-by-ref"      # managed by clang-tidy: performance-unnecessary-value-param
      # "implicit-casts"            # managed by clang-tidy: bugprone-bool-pointer-implicit-conversion
      # "returning-void-expression" # managed by clang-tidy: readability-avoid-return-with-void-value
      # "rule-of-three"             # managed by clang-tidy: cppcoreguidelines-special-member-functions
      # "virtual-call-ctor"         # managed by clang-tidy: clang-analyzer-cplusplus.PureVirtualCall/clang-analyzer-optin.cplusplus.VirtualCall
      "base-class-event"
      "copyable-polymorphic"
      "ctor-missing-parent-argument"
      "function-args-by-value"
      "missing-qobject-macro"
      "old-style-connect"
      "qstring-allocations"
      "static-pmf"
    ];
    CLAZY_FIXIT = builtins.concatStringsSep "," [
      # Checks from Manual Level:
      "fix-qbytearray-conversion-to-c-style"
      "fix-missing-qstringref"
      "fix-qt-keyword-emit"
      "fix-qt-keywords"
      "fix-sanitize-inline-keyword"

      # Checks from Level 0:
      "fix-fully-qualified-moc-types"
      "fix-qcolor-from-literal"
      "fix-qdatetime-utc"
      "fix-qfileinfo-exists"
      "fix-qgetenv"

      # Checks from Level 1:
      "fix-auto-unexpected-qstringbuilder"
      "fix-range-loop-add-qasconst"
      "fix-range-loop-add-ref"

      # Checks from Level 2:
      # "fix-function-args-by-ref" # check managed by clang-tidy
      "fix-missing-qobject-macro"
      "fix-old-style-connect"
    ];
  };

  packages = [
    pkgs.libglvnd
    (pkgs.buildEnv {
      name = "clang-tools+";
      paths = [
        pkgs.clang-tools
        (pkgs.runCommandLocal "run-clang-tidy" { } ''
          mkdir -p "$out/bin"
          ln -s "${pkgs.libclang.out}/bin/run-clang-tidy" "$out/bin/run-clang-tidy"
        '')
      ];
      pathsToLink = [
        "/bin"
      ];
    })
    (pkgs.buildEnv {
      name = "qt-build-env";
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
    })
  ];

  languages.cplusplus = {
    enable = true;
    lsp.package = pkgs.clang-tools;
  };
}
