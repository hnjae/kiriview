# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ pkgs, ... }:
{
  scripts."run-clazy-parallel" = {
    description = "Run clazy-standalone in parallel with ordered output";
    packages = [ pkgs.coreutils ];
    exec = # sh
      ''
        set -euo pipefail

        kiriview_default_local_jobs() {
            local cpu_count
            cpu_count="$(nproc)"
            printf '%d\n' "$((cpu_count / 2 + 1))"
        }

        usage() {
            cat >&2 <<'EOF'
        Usage: run-clazy-parallel [options] -- <source>...

        Options:
          --jobs <count>           Number of clazy-standalone jobs to run in parallel.
          --clazy-binary <path>    clazy-standalone binary to execute.
          --checks <checks>        clazy checks to run.
          --header-filter <regex>  Headers to diagnose.
          --ignore-dirs <regex>    Directories for clazy to ignore.
          --export-fixes-dir <dir> Directory for per-source exported fixit YAML files.
          -p, --compile-db <dir>   Compilation database directory.
        EOF
        }

        jobs="''${CLAZY_JOBS:-}"
        clazy_binary="''${CLAZY_BINARY:-clazy-standalone}"
        checks="''${CLAZY_CHECKS:-level0}"
        header_filter=""
        ignore_dirs=""
        export_fixes_dir=""
        compile_db="."

        while (($# > 0)); do
            case "$1" in
            --jobs)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                jobs="$2"
                shift 2
                ;;
            --jobs=*)
                jobs="''${1#--jobs=}"
                shift
                ;;
            --clazy-binary)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                clazy_binary="$2"
                shift 2
                ;;
            --clazy-binary=*)
                clazy_binary="''${1#--clazy-binary=}"
                shift
                ;;
            --checks)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                checks="$2"
                shift 2
                ;;
            --checks=*)
                checks="''${1#--checks=}"
                shift
                ;;
            --header-filter)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                header_filter="$2"
                shift 2
                ;;
            --header-filter=*)
                header_filter="''${1#--header-filter=}"
                shift
                ;;
            --ignore-dirs)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                ignore_dirs="$2"
                shift 2
                ;;
            --ignore-dirs=*)
                ignore_dirs="''${1#--ignore-dirs=}"
                shift
                ;;
            --export-fixes-dir)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                export_fixes_dir="$2"
                shift 2
                ;;
            --export-fixes-dir=*)
                export_fixes_dir="''${1#--export-fixes-dir=}"
                shift
                ;;
            -p | --compile-db)
                if (($# < 2)); then
                    usage
                    exit 2
                fi
                compile_db="$2"
                shift 2
                ;;
            --compile-db=*)
                compile_db="''${1#--compile-db=}"
                shift
                ;;
            --help)
                usage
                exit 0
                ;;
            --)
                shift
                break
                ;;
            *)
                usage
                exit 2
                ;;
            esac
        done

        if [[ -z $jobs ]]; then
            if [[ -n ''${KIRIVIEW_JOBS:-} ]]; then
                jobs="$KIRIVIEW_JOBS"
            else
                jobs="$(kiriview_default_local_jobs)"
            fi
        fi
        if ! [[ $jobs =~ ^[0-9]+$ ]] || ((jobs < 1)); then
            printf 'Invalid --jobs value: %s\n' "$jobs" >&2
            exit 2
        fi
        if [[ -n $export_fixes_dir ]]; then
            mkdir -p "$export_fixes_dir"
        fi

        sources=("$@")
        source_count="$#"
        if ((source_count == 0)); then
            exit 0
        fi

        tmp_dir="$(mktemp -d)"
        trap 'rm -rf "$tmp_dir"' EXIT

        run_clazy_for_source() {
            local index="$1"
            local source="$2"
            local output_file="$tmp_dir/$index.out"
            local status_file="$tmp_dir/$index.status"
            local status

            set +e
            {
                printf '[%d/%d] Processing file %s.\n' "$((index + 1))" "$source_count" "$source"
                clazy_args=(
                    --checks="$checks"
                    --extra-arg=-Werror
                    -p "$compile_db"
                )
                if [[ -n $header_filter ]]; then
                    clazy_args+=(--header-filter="$header_filter")
                fi
                if [[ -n $ignore_dirs ]]; then
                    clazy_args+=(--ignore-dirs="$ignore_dirs")
                fi
                if [[ -n $export_fixes_dir ]]; then
                    clazy_args+=(--export-fixes="$export_fixes_dir/$index.yaml")
                fi
                "$clazy_binary" "''${clazy_args[@]}" "$source"
            } >"$output_file" 2>&1
            status="$?"
            printf '%s\n' "$status" >"$status_file"
            exit 0
        }

        printf 'Running clazy-standalone for %d files with %d jobs...\n' "$source_count" "$jobs"

        running_jobs=0
        for ((index = 0; index < source_count; index++)); do
            run_clazy_for_source "$index" "''${sources[$index]}" &
            running_jobs=$((running_jobs + 1))
            if ((running_jobs >= jobs)); then
                wait -n
                running_jobs=$((running_jobs - 1))
            fi
        done

        while ((running_jobs > 0)); do
            wait -n
            running_jobs=$((running_jobs - 1))
        done

        exit_status=0
        for ((index = 0; index < source_count; index++)); do
            output_file="$tmp_dir/$index.out"
            status_file="$tmp_dir/$index.status"

            if [[ -f $output_file ]]; then
                cat "$output_file"
            fi

            if [[ ! -f $status_file ]]; then
                printf 'clazy-standalone did not report a status for %s.\n' "''${sources[$index]}" >&2
                exit_status=1
                continue
            fi

            read -r source_status <"$status_file"
            if ((source_status != 0)); then
                exit_status=1
            fi
        done

        exit "$exit_status"
      '';
  };
}
