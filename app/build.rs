// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use cxx_gen::{HEADER, Opt, generate_header_and_cc};
use proc_macro2::TokenStream;
use std::{
    collections::BTreeSet,
    env, fs,
    path::{Component, Path, PathBuf},
};

const RUST_BRIDGE_SOURCES_FILE: &str = "src/rust_support_bridge_sources.txt";

fn main() {
    println!("cargo::rerun-if-env-changed=KIRIVIEW_CXXBRIDGE_OUTPUT_DIR");
    println!("cargo::rerun-if-env-changed=KIRIVIEW_CXXBRIDGE_TRIGGER");
    if let Some(trigger) = env::var_os("KIRIVIEW_CXXBRIDGE_TRIGGER") {
        println!(
            "cargo::rerun-if-changed={}",
            PathBuf::from(trigger).display()
        );
    }

    let output_root = env::var_os("KIRIVIEW_CXXBRIDGE_OUTPUT_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR must be set")).join("cxxbridge")
        });

    write_if_changed(&output_root.join("rust/cxx.h"), HEADER.as_bytes());
    for source in source_manifest(RUST_BRIDGE_SOURCES_FILE) {
        generate_bridge(Path::new(&source), &output_root);
    }
}

fn generate_bridge(source: &Path, output_root: &Path) {
    println!("cargo::rerun-if-changed={}", source.display());
    let source_text = fs::read_to_string(source)
        .unwrap_or_else(|error| panic!("failed to read {}: {error}", source.display()));
    let tokens: TokenStream = source_text
        .parse()
        .unwrap_or_else(|error| panic!("failed to parse {}: {error}", source.display()));
    let generated = generate_header_and_cc(tokens, &Opt::default()).unwrap_or_else(|error| {
        panic!(
            "failed to generate bridge for {}: {error}",
            source.display()
        )
    });

    let relative = source.strip_prefix("src").unwrap_or(source);
    let stem = relative.with_extension("");
    let include_path = output_root
        .join("kiriview/src")
        .join(&stem)
        .with_extension("cxx.h");
    let source_path = output_root
        .join("sources")
        .join(&stem)
        .with_extension("cxx.cpp");
    write_if_changed(&include_path, &generated.header);
    write_if_changed(&source_path, &generated.implementation);
}

fn source_manifest(path: &str) -> Vec<String> {
    println!("cargo::rerun-if-changed={path}");
    let mut seen = BTreeSet::new();
    fs::read_to_string(path)
        .unwrap_or_else(|error| panic!("failed to read source manifest {path}: {error}"))
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty() && !line.starts_with('#'))
        .map(|line| {
            validate_source_manifest_path(path, line);
            if !seen.insert(line.to_owned()) {
                panic!("duplicate source manifest entry {line} in {path}");
            }
            line.to_owned()
        })
        .collect()
}

fn validate_source_manifest_path(manifest: &str, source: &str) {
    let path = Path::new(source);
    if path.is_absolute()
        || path.components().any(|component| {
            matches!(
                component,
                Component::ParentDir | Component::RootDir | Component::Prefix(_)
            )
        })
        || !path.starts_with("src/support")
        || path.extension().and_then(|value| value.to_str()) != Some("rs")
        || !path.exists()
    {
        panic!("invalid Rust bridge source {source} in {manifest}");
    }
}

fn write_if_changed(path: &Path, contents: &[u8]) {
    if fs::read(path).is_ok_and(|existing| existing == contents) {
        return;
    }
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .unwrap_or_else(|error| panic!("failed to create {}: {error}", parent.display()));
    }
    fs::write(path, contents)
        .unwrap_or_else(|error| panic!("failed to write {}: {error}", path.display()));
}
