# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ config, ... }:
let
  repoRoot = config.devenv.root;
in
{
  _module.args.kiriviewApp = {
    inherit repoRoot;
    appRoot = "${repoRoot}/app";
  };
}
