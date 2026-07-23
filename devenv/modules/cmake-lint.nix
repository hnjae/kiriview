# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  tasks."ci:repo:lint:cmake" = {
    description = "Lint repository CMake sources";
    showOutput = true;
    before = [ "ci:lint" ];
    exec = # sh
      ''
        set -euo pipefail

        mapfile -d "" cmake_files < <(
            git ls-files -z '*CMakeLists.txt' '*.cmake' '*.cmake.in'
        )
        cmake-lint "''${cmake_files[@]}"
      '';
  };
}
