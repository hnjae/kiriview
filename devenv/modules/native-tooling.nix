# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ pkgs, ... }:
let
  gccCxxInclude = "${pkgs.gcc.cc}/include/c++/${pkgs.gcc.version}";
in
{
  packages = [
    pkgs.clazy
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
    pkgs.cmake-format
    pkgs.clang-tools
    pkgs.libglvnd
    (pkgs.runCommandLocal "run-clang-tidy" { } ''
      mkdir -p "$out/bin"
      ln -s "${pkgs.libclang.out}/bin/run-clang-tidy" "$out/bin/run-clang-tidy"
    '')
  ];

  languages.cplusplus = {
    enable = true;
    lsp.package = pkgs.clang-tools;
  };

  # clang-tidy does not infer the wrapped GCC standard library include paths
  # from CMake's compilation database.
  env.CPLUS_INCLUDE_PATH = builtins.concatStringsSep ":" [
    gccCxxInclude
    "${gccCxxInclude}/${pkgs.stdenv.hostPlatform.config}"
    "${pkgs.glibc.dev}/include"
  ];

  enterShell = # sh
    ''
      declare -A seen_cmake_prefix_paths=()
      cmake_prefix_path=""
      add_cmake_prefix_path() {
          local prefix_path=$1

          if [[ -z $prefix_path || ! -d $prefix_path || -v 'seen_cmake_prefix_paths[$prefix_path]' ]]; then
              return
          fi

          seen_cmake_prefix_paths[$prefix_path]=1
          cmake_prefix_path="''${cmake_prefix_path:+$cmake_prefix_path:}$prefix_path"
      }

      add_cmake_prefix_path "$DEVENV_PROFILE"
      IFS=: read -r -a existing_cmake_prefix_paths <<<"''${CMAKE_PREFIX_PATH:-}"
      for prefix_path in "''${existing_cmake_prefix_paths[@]}"; do
          add_cmake_prefix_path "$prefix_path"
      done
      export CMAKE_PREFIX_PATH="$cmake_prefix_path"
      unset seen_cmake_prefix_paths cmake_prefix_path prefix_path
      unset -f add_cmake_prefix_path
    '';
}
