# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ lib, pkgs, ... }:
{
  env.CLAZY_CHECKS = builtins.concatStringsSep "," [
    # Checks from Manual Level:
    # "assert-with-side-effects" # managed by clang-tidy: bugprone-assert-side-effect
    # "unneeded-cast" # too many false positives; its generic cases partially overlap clang-tidy
    # "unused-result-check" # too broad for const methods with intentional side effects
    "ifndef-define-typo"
    "qbytearray-conversion-to-c-style"
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
    # False-positives on already-reserved std::vector caches.
    # "reserve-candidates"
    "use-chrono-in-qtimer"
    "use-arrow-operator-instead-of-data"
    "used-qunused-variable"

    # Checks from Level 0-1:
    "level1"
    "no-range-loop-reference"
    "no-rule-of-two-soft"

    # Checks from Level 2:
    # "function-args-by-ref"      # too noisy for Qt callback and implicitly shared value patterns
    "implicit-casts"
    # "returning-void-expression" # managed by clang-tidy: readability-avoid-return-with-void-value
    # "virtual-call-ctor"         # managed by clang-tidy: clang-analyzer-cplusplus.PureVirtualCall/clang-analyzer-optin.cplusplus.VirtualCall
    "base-class-event"
    "copyable-polymorphic"
    # QQuickImageProvider is not a QObject-parent ownership surface.
    # "ctor-missing-parent-argument"
    # Flags virtual override signatures and shared interfaces.
    # "function-args-by-value"
    # Local QObject helpers that do not use signals, slots, or properties do not need moc metadata.
    # "missing-qobject-macro"
    # Dynamic QObject/QML signals are not always available as typed C++ signals.
    # "old-style-connect"
    "qstring-allocations"
    "static-pmf"
  ];

  env.CLAZY_FIXIT = builtins.concatStringsSep "," [
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

    # Checks from Level 2:
    # "fix-function-args-by-ref" # disabled with the corresponding clazy check
    # "fix-missing-qobject-macro" # Q_OBJECT additions require matching moc build metadata
    # "fix-old-style-connect"
  ];

  tasks."ci:repo:lint:clang-tidy-config" = {
    description = "Validate the repository clang-tidy configuration";
    before = [ "ci:lint" ];
    exec = "${lib.getExe' pkgs.clang-tools "clang-tidy"} --verify-config --config-file=.clang-tidy";
  };
}
