# Flatpak Development Builds

These instructions install the current source checkout as a local Flatpak development build. This is not a stable release build.

## Prerequisites

1. Install `flatpak` and `flatpak-builder`.
1. Add Flathub if it is not already configured:

   ```sh
   flatpak --user remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
   ```

## Build and Install

Build and install KiriView into your user Flatpak installation:

```sh
flatpak-builder \
    --default-branch main \
    --user \
    --install \
    --install-deps-from=flathub \
    --ccache \
    --keep-build-dirs \
    --disable-tests \
    --force-clean build-dir \
    org.hnjae.kiriview.json
```

The same development build can also be installed through the repository task:

```sh
just install
```

Run the installed development build:

```sh
flatpak run org.hnjae.kiriview
```

To update the installed development build after changing or pulling the source, rerun the build and install command above.

To remove the local development build:

```sh
flatpak --user uninstall org.hnjae.kiriview
```

## Packaging Notes

### HEIF codecs

The Flatpak manifest intentionally builds libheif as an application module instead of relying on the Freedesktop or KDE runtime copy. The runtime libheif and codec plugins have historically lagged the HEIF still-image and image sequence support KiriView needs, including JPEG, JPEG 2000, AVC/H.264, HEVC/H.265, and VVC/H.266 HEIF variants.

When updating the Flatpak runtime, SDK, SDK extensions, or codec plugin assumptions, re-check whether the bundled libheif module is still required before removing or changing it. At minimum, verify the current runtime libheif version, HEIF sequence decoding, alpha handling, and `just build-flatpak-with-test`.

### SMB KIO worker

KiriView bundles the SMB KIO worker because the KDE runtime can build `kio-extras` without it when Samba is unavailable. Build and pin its upstream dependency chain in this order: Parse-Yapp, rpcsvc-proto (`rpcgen`), Samba (`libsmbclient`), KDSoap, KDSoap WS Discovery Client, and `kio-extras`. Parse-Yapp and rpcsvc-proto are build-only inputs. The upstream worker requires the discovery libraries at build time even though opening a direct SMB file does not use network-root discovery.

Keep the bundled `kio-extras` source at the same revision as the selected KDE runtime's `kio-extras` module. Update the runtime and bundled copy together, then verify that the application-installed worker loads against the runtime KIO libraries.

The SMB-specific sandbox surface is `--share=network` for transport and `--talk-name=org.kde.kpasswdserver6` for the standard KIO authentication prompt and credential cache. Do not add direct KWallet, `org.kde.kiod6`, or Avahi access for direct-file support; network-root discovery is a separate capability. Verify that the password service can be activated on each supported host environment, and never put credentials in test URLs or logs.

Treat Samba as a security-sensitive network parser. Pin release archives by checksum, retain update-check metadata and required license notices, monitor Samba security advisories, and rebuild promptly for applicable fixes. After changing the runtime or SMB bundle, run the Flatpak permission lint and a Flatpak build, confirm that the `smb` worker is discoverable, exercise guest and authenticated URLs using both an IP address and a `.local` host, and audit session- and system-bus denials before widening permissions.
