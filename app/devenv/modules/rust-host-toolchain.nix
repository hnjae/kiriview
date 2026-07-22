# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later
{
  pkgs,
  lib,
  ...
}:
let
  rustHostLldBin = pkgs.runCommandLocal "kiriview-rust-host-lld-bin" { } ''
    mkdir -p "$out/bin"
    ln -s ${lib.getExe' pkgs.lld "ld.lld"} "$out/bin/ld"
    ln -s ${lib.getExe' pkgs.lld "ld.lld"} "$out/bin/ld.lld"
    ln -s ${lib.getExe' pkgs.lld "lld"} "$out/bin/lld"
  '';
  rustHostLinker = pkgs.writeShellApplication {
    name = "kiriview-rust-host-linker";
    text = ''
      filtered_args=()
      for arg in "$@"; do
          case "$arg" in
              -fuse-ld=gold)
                  ;;
              *)
                  filtered_args+=("$arg")
                  ;;
          esac
      done

      exec ${lib.getExe' pkgs.stdenv.cc "cc"} -B${rustHostLldBin}/bin/ "''${filtered_args[@]}"
    '';
  };
  rustHostLinkerFlag = "-C linker=${lib.getExe rustHostLinker}";
  environment = # sh
    ''
      case " ''${RUSTFLAGS:-} " in
      *" ${rustHostLinkerFlag} "*)
          ;;
      *)
          export RUSTFLAGS="''${RUSTFLAGS:+$RUSTFLAGS }${rustHostLinkerFlag}"
          ;;
      esac
    '';
in
{
  _module.args.rustHost = {
    inherit environment;
  };
}
