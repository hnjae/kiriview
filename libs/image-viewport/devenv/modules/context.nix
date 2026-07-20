# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{ config, ... }:
{
  _module.args.kiriviewImageViewport = {
    root = "${config.devenv.root}/libs/image-viewport";
  };
}
