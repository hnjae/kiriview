#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly repo_root
mapfile -t application_id_values < <(
    sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' "$repo_root/application-id.txt"
)
if ((${#application_id_values[@]} != 1)); then
    printf 'application-id.txt must declare exactly one application ID.\n' >&2
    exit 1
fi
readonly application_id="${application_id_values[0]}"

if [[ ! $application_id =~ ^[a-z][a-z0-9]*(\.[a-z][a-z0-9]*)+$ ]]; then
    printf 'Invalid application ID: %s\n' "$application_id" >&2
    exit 1
fi

while IFS= read -r qml_import; do
    case "$qml_import" in
    *.kiriview)
        if [[ $qml_import != "$application_id" ]]; then
            printf 'QML application module import %s does not match canonical application ID %s.\n' \
                "$qml_import" "$application_id" >&2
            exit 1
        fi
        ;;
    esac
done < <(
    sed -n 's/^[[:space:]]*import[[:space:]]\+\([^[:space:]]\+\).*$/\1/p' \
        "$repo_root"/src/qml/*.qml
)

readonly manifest="$repo_root/$application_id.json"
readonly desktop_file="$repo_root/$application_id.desktop"
readonly icon_file="$repo_root/data/icons/hicolor/scalable/apps/$application_id.svg"
for required_file in "$manifest" "$desktop_file" "$icon_file"; do
    if [[ ! -f $required_file ]]; then
        printf 'Application identity artifact is missing: %s\n' "$required_file" >&2
        exit 1
    fi
done

mapfile -t desktop_artifacts < <(find "$repo_root" -maxdepth 1 -type f -name '*.desktop')
mapfile -t icon_artifacts < <(
    find "$repo_root/data/icons/hicolor/scalable/apps" -maxdepth 1 -type f -name '*.svg'
)
if ((${#desktop_artifacts[@]} != 1)) || [[ ${desktop_artifacts[0]} != "$desktop_file" ]] ||
    ((${#icon_artifacts[@]} != 1)) || [[ ${icon_artifacts[0]} != "$icon_file" ]]; then
    printf 'Installed desktop or icon artifacts introduce a non-canonical identity.\n' >&2
    exit 1
fi

while IFS= read -r metadata_file; do
    metadata_id="$(jq --raw-output '.id? | strings' "$metadata_file")"
    if [[ -n $metadata_id && $metadata_id != "$application_id" ]]; then
        printf 'Metadata ID %s in %s does not match canonical application ID %s.\n' \
            "$metadata_id" "$metadata_file" "$application_id" >&2
        exit 1
    fi
done < <(find "$repo_root" -maxdepth 1 -type f -name '*.json')

manifest_id="$(jq --exit-status --raw-output '.id | strings' "$manifest")"
if [[ $manifest_id != "$application_id" ]]; then
    printf 'Flatpak ID %s does not match canonical application ID %s.\n' \
        "$manifest_id" "$application_id" >&2
    exit 1
fi

mapfile -t desktop_icons < <(sed -n 's/^Icon=//p' "$desktop_file")
if ((${#desktop_icons[@]} != 1)) || [[ ${desktop_icons[0]} != "$application_id" ]]; then
    printf 'Desktop icon identity does not match canonical application ID %s.\n' \
        "$application_id" >&2
    exit 1
fi

mapfile -t application_build_commands < <(
    jq --exit-status --raw-output \
        '.modules[] | select(.name == "kiriview") | .["build-commands"][]' \
        "$manifest"
)
joined_build_commands="$(printf '%s\n' "${application_build_commands[@]}")"
readonly joined_build_commands
if ! grep -Fq "target/i18n/$application_id.desktop" <<<"$joined_build_commands" ||
    ! grep -Fq "/app/share/applications/$application_id.desktop" \
        <<<"$joined_build_commands"; then
    printf 'Flatpak desktop installation does not match canonical application ID %s.\n' \
        "$application_id" >&2
    exit 1
fi
