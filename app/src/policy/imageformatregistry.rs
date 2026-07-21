// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::archiveformat::supported_comic_book_archive_extensions;
use crate::fileextension::extension_for_file_name;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum ImageFormatDecoderFamily {
    Svg,
    HeifFamily,
    Raw,
    QtRaster,
}

struct ImageFormat {
    extensions: &'static [&'static str],
    decoder_family: ImageFormatDecoderFamily,
}

const RAW_IMAGE_EXTENSIONS: &[&str] = &[
    "3fr", "arw", "bay", "bmq", "cr2", "cr3", "crw", "cs1", "cs2", "dcr", "dng", "erf", "fff",
    "iiq", "k25", "kdc", "mdc", "mef", "mos", "mrw", "nef", "nrw", "orf", "pef", "raf", "raw",
    "rdc", "rwl", "rw2", "sr2", "srf", "srw", "x3f",
];

const SUPPORTED_IMAGE_FORMATS: &[ImageFormat] = &[
    ImageFormat {
        extensions: &["png"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["jpeg", "jpg"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["jp2"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["jxl"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["gif"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["webp"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["avif"],
        decoder_family: ImageFormatDecoderFamily::HeifFamily,
    },
    ImageFormat {
        extensions: &["avifs"],
        decoder_family: ImageFormatDecoderFamily::HeifFamily,
    },
    ImageFormat {
        extensions: &["avci"],
        decoder_family: ImageFormatDecoderFamily::HeifFamily,
    },
    ImageFormat {
        extensions: &["heic", "heif", "hif"],
        decoder_family: ImageFormatDecoderFamily::HeifFamily,
    },
    ImageFormat {
        extensions: &["heics", "heifs"],
        decoder_family: ImageFormatDecoderFamily::HeifFamily,
    },
    ImageFormat {
        extensions: &["hej2"],
        decoder_family: ImageFormatDecoderFamily::HeifFamily,
    },
    ImageFormat {
        extensions: &["bmp"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: &["tif", "tiff"],
        decoder_family: ImageFormatDecoderFamily::QtRaster,
    },
    ImageFormat {
        extensions: RAW_IMAGE_EXTENSIONS,
        decoder_family: ImageFormatDecoderFamily::Raw,
    },
    ImageFormat {
        extensions: &["svg"],
        decoder_family: ImageFormatDecoderFamily::Svg,
    },
];

#[cxx::bridge(namespace = "kiriview")]
mod ffi {
    extern "Rust" {
        #[cxx_name = "rustSupportedOpenExtensions"]
        fn rust_supported_open_extensions() -> Vec<String>;

        #[cxx_name = "rustIsSupportedImageFileName"]
        fn rust_is_supported_image_file_name(name: &str) -> bool;

    }
}

pub(crate) fn supported_image_extensions() -> Vec<String> {
    unique_sorted_strings(
        SUPPORTED_IMAGE_FORMATS
            .iter()
            .flat_map(|format| format.extensions.iter().copied()),
    )
}

#[cfg(test)]
pub(crate) fn raw_image_extensions() -> Vec<String> {
    unique_sorted_strings(RAW_IMAGE_EXTENSIONS.iter().copied())
}

pub(crate) fn is_supported_raw_image_extension(extension: &str) -> bool {
    decoder_family_for_supported_image_extension(extension) == Some(ImageFormatDecoderFamily::Raw)
}

pub(crate) fn decoder_family_for_supported_image_extension(
    extension: &str,
) -> Option<ImageFormatDecoderFamily> {
    SUPPORTED_IMAGE_FORMATS
        .iter()
        .find(|format| format.extensions.contains(&extension))
        .map(|format| format.decoder_family)
}

fn rust_supported_open_extensions() -> Vec<String> {
    let mut extensions = supported_image_extensions();
    extensions.extend(supported_comic_book_archive_extensions());
    extensions.sort();
    extensions
}

fn rust_is_supported_image_file_name(name: &str) -> bool {
    is_supported_image_file_name(name)
}

pub(crate) fn is_supported_image_file_name(name: &str) -> bool {
    extension_for_file_name(name)
        .is_some_and(|extension| image_extension_is_supported(extension.as_str()))
}

fn image_extension_is_supported(extension: &str) -> bool {
    SUPPORTED_IMAGE_FORMATS
        .iter()
        .any(|format| format.extensions.contains(&extension))
}

fn unique_sorted_strings(values: impl IntoIterator<Item = &'static str>) -> Vec<String> {
    let mut strings: Vec<String> = values.into_iter().map(str::to_owned).collect();
    strings.sort();
    strings.dedup();
    strings
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn supported_open_extensions_include_images_and_comic_books_only() {
        let extensions = rust_supported_open_extensions();

        assert_eq!(extensions, {
            let mut sorted = extensions.clone();
            sorted.sort();
            sorted
        });
        assert!(extensions.contains(&"png".to_owned()));
        assert!(extensions.contains(&"cbz".to_owned()));
        assert!(!extensions.contains(&"zip".to_owned()));
        assert!(!extensions.contains(&"rar".to_owned()));
    }

    #[test]
    fn matches_supported_image_file_names_case_insensitively() {
        assert!(is_supported_image_file_name("photo.png"));
        assert!(is_supported_image_file_name("photo.JPG"));
        assert!(is_supported_image_file_name("scan.DnG"));
        assert!(is_supported_image_file_name(
            "zip:///path/archive.cbz!/page.SVG"
        ));
    }

    #[test]
    fn excludes_hidden_trailing_dot_and_non_image_file_names() {
        for name in [
            ".png",
            "photo.",
            "photo",
            "clip.mp4",
            "book.cbz",
            "archive.zip",
            "zip:///path/archive.cbz!/chapter/",
        ] {
            assert!(!is_supported_image_file_name(name), "{name}");
        }
    }

    #[test]
    fn format_capability_catalog_covers_every_advertised_extension() {
        for extension in supported_image_extensions() {
            assert!(
                decoder_family_for_supported_image_extension(&extension).is_some(),
                "advertised extension {extension} should have a decoder family"
            );
        }
    }

    #[test]
    fn raw_image_extensions_are_owned_by_format_catalog() {
        let raw_extensions = raw_image_extensions();
        assert_eq!(
            raw_extensions,
            vec![
                "3fr", "arw", "bay", "bmq", "cr2", "cr3", "crw", "cs1", "cs2", "dcr", "dng", "erf",
                "fff", "iiq", "k25", "kdc", "mdc", "mef", "mos", "mrw", "nef", "nrw", "orf", "pef",
                "raf", "raw", "rdc", "rw2", "rwl", "sr2", "srf", "srw", "x3f",
            ]
        );

        for extension in raw_extensions {
            assert_eq!(
                decoder_family_for_supported_image_extension(&extension),
                Some(ImageFormatDecoderFamily::Raw),
                "raw extension {extension} should route to the RAW decoder family"
            );
        }
        assert_ne!(
            decoder_family_for_supported_image_extension("tif"),
            Some(ImageFormatDecoderFamily::Raw)
        );
    }
}
