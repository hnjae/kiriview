#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

set -euo pipefail

export LC_ALL=C

readonly capability_manifest="src/format/supportedmediamimetypes.inc"
readonly desktop_file="org.hnjae.kiriview.desktop"
check_tmp_dir="$(mktemp -d)"
readonly check_tmp_dir
trap 'rm -rf -- "${check_tmp_dir}"' EXIT

sed -En \
    -e 's/^[[:space:]]*KIRIVIEW_(IMAGE|DIRECT_VIDEO)_MIME_TYPE\("([^"]+)"\)[[:space:]]*$/\2/p' \
    -e 's/^[[:space:]]*KIRIVIEW_COMIC_ARCHIVE_MIME_TYPE\("[^"]+", "([^"]+)"\)[[:space:]]*$/\1/p' \
    "$capability_manifest" >"${check_tmp_dir}/owned"

mime_line_count="$(sed -n '/^MimeType=/p' "$desktop_file" | wc -l)"
if [[ $mime_line_count -ne 1 ]]; then
    printf 'Desktop metadata must contain exactly one MimeType entry\n' >&2
    exit 1
fi

sed -n 's/^MimeType=//p' "$desktop_file" |
    tr ';' '\n' |
    sed '/^$/d' >"${check_tmp_dir}/advertised"

for capability_set in owned advertised; do
    if [[ ! -s "${check_tmp_dir}/${capability_set}" ]]; then
        printf 'Supported media %s MIME set must not be empty\n' "$capability_set" >&2
        exit 1
    fi
    sort "${check_tmp_dir}/${capability_set}" >"${check_tmp_dir}/${capability_set}.sorted"
    sort "${check_tmp_dir}/${capability_set}" |
        uniq -d >"${check_tmp_dir}/${capability_set}.duplicates"
    if [[ -s "${check_tmp_dir}/${capability_set}.duplicates" ]]; then
        printf 'Supported media %s MIME set contains duplicates:\n' "$capability_set" >&2
        sed 's/^/  /' "${check_tmp_dir}/${capability_set}.duplicates" >&2
        exit 1
    fi
done

if ! diff -u --label owned-capabilities --label desktop-advertisement \
    "${check_tmp_dir}/owned.sorted" "${check_tmp_dir}/advertised.sorted"; then
    printf 'Desktop MIME advertisement must exactly project the owned media capabilities\n' >&2
    exit 1
fi
