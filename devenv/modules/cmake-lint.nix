# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  tasks."ci:repo:lint:cmake" = {
    description = "Check repository CMake policy and lint CMake sources";
    showOutput = true;
    before = [ "ci:lint" ];
    exec = # sh
      ''
        set -euo pipefail

        read_cmake_minimum() {
            local path="$1"
            local versions
            mapfile -t versions < <(
                sed -nE \
                    's|^[[:space:]]*cmake_minimum_required\([[:space:]]*VERSION[[:space:]]+([0-9]+([.][0-9]+)*).*|\1|p' \
                    "$path"
            )
            if ((''${#versions[@]} != 1)); then
                printf '%s must declare exactly one CMake minimum version.\n' "$path" >&2
                return 1
            fi
            printf '%s\n' "''${versions[0]}"
        }

        application_cmake_minimum="$(read_cmake_minimum app/CMakeLists.txt)"
        mapfile -d "" -t component_roots < <(
            git ls-files -z ':(glob)libs/*/CMakeLists.txt'
        )
        for component_root in "''${component_roots[@]}"; do
            component_cmake_minimum="$(read_cmake_minimum "$component_root")"
            if [[ "$component_cmake_minimum" != "$application_cmake_minimum" ]]; then
                printf \
                    '%s requires CMake %s, but the application baseline is %s.\n' \
                    "$component_root" \
                    "$component_cmake_minimum" \
                    "$application_cmake_minimum" >&2
                exit 1
            fi
        done

        mapfile -d "" cmake_files < <(
            git ls-files -z '*CMakeLists.txt' '*.cmake' '*.cmake.in'
        )
        cmake-lint "''${cmake_files[@]}"
      '';
  };
}
