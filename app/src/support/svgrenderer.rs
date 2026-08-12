// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use resvg::{
    tiny_skia::{Pixmap, Transform},
    usvg::{ImageHrefResolver, Options, Tree},
};

#[cxx::bridge]
mod ffi {
    #[namespace = "kiriview"]
    #[derive(Clone, Copy, Debug, PartialEq)]
    struct RustSvgImageSize {
        valid: bool,
        width: i32,
        height: i32,
    }

    #[namespace = "kiriview"]
    extern "Rust" {
        #[cxx_name = "rustSvgIntrinsicSize"]
        fn rust_svg_intrinsic_size(data: &[u8]) -> RustSvgImageSize;

        #[cxx_name = "rustRenderSvgImage"]
        fn rust_render_svg_image(data: &[u8], width: i32, height: i32) -> Vec<u8>;

    }
}

use ffi::RustSvgImageSize;

fn rust_svg_intrinsic_size(data: &[u8]) -> RustSvgImageSize {
    svg_intrinsic_size(data)
}

fn rust_render_svg_image(data: &[u8], width: i32, height: i32) -> Vec<u8> {
    render_svg_image(data, width, height).unwrap_or_default()
}

fn svg_intrinsic_size(data: &[u8]) -> RustSvgImageSize {
    let Some(tree) = parse_svg_tree(data) else {
        return invalid_svg_image_size();
    };

    rust_svg_image_size(
        tree.size().to_int_size().width(),
        tree.size().to_int_size().height(),
    )
}

fn render_svg_image(data: &[u8], width: i32, height: i32) -> Option<Vec<u8>> {
    let (width, height) = positive_dimensions(width, height)?;
    let tree = parse_svg_tree(data)?;
    let transform = image_transform(&tree, width, height);

    render_tree(&tree, width, height, transform)
}

fn parse_svg_tree(data: &[u8]) -> Option<Tree> {
    if data.starts_with(&[0x1f, 0x8b]) {
        return None;
    }

    let options = svg_options();
    Tree::from_data(data, &options).ok()
}

fn svg_options() -> Options<'static> {
    Options {
        resources_dir: None,
        image_href_resolver: ImageHrefResolver {
            resolve_data: ImageHrefResolver::default_data_resolver(),
            resolve_string: Box::new(|_, _| None),
        },
        ..Options::default()
    }
}

fn image_transform(tree: &Tree, width: u32, height: u32) -> Transform {
    let svg_size = tree.size();
    Transform::from_scale(
        width as f32 / svg_size.width(),
        height as f32 / svg_size.height(),
    )
}

fn render_tree(tree: &Tree, width: u32, height: u32, transform: Transform) -> Option<Vec<u8>> {
    let mut pixmap = Pixmap::new(width, height)?;
    resvg::render(tree, transform, &mut pixmap.as_mut());
    Some(pixmap.take())
}

fn positive_dimensions(width: i32, height: i32) -> Option<(u32, u32)> {
    Some((u32::try_from(width).ok()?, u32::try_from(height).ok()?))
        .filter(|(width, height)| *width > 0 && *height > 0 && width.checked_mul(*height).is_some())
}

fn rust_svg_image_size(width: u32, height: u32) -> RustSvgImageSize {
    let Some(width) = i32::try_from(width).ok() else {
        return invalid_svg_image_size();
    };
    let Some(height) = i32::try_from(height).ok() else {
        return invalid_svg_image_size();
    };

    RustSvgImageSize {
        valid: true,
        width,
        height,
    }
}

fn invalid_svg_image_size() -> RustSvgImageSize {
    RustSvgImageSize {
        valid: false,
        width: 0,
        height: 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn reports_valid_intrinsic_size() {
        let size = svg_intrinsic_size(
            br#"<svg xmlns="http://www.w3.org/2000/svg" width="80" height="40"/>"#,
        );

        assert_eq!(
            size,
            RustSvgImageSize {
                valid: true,
                width: 80,
                height: 40,
            }
        );
    }

    #[test]
    fn rejects_invalid_source_and_target_dimensions() {
        const SVG: &[u8] = br#"<svg xmlns="http://www.w3.org/2000/svg" width="12" height="8"/>"#;

        assert!(!svg_intrinsic_size(b"not svg").valid);
        assert!(render_svg_image(b"not svg", 12, 8).is_none());
        assert!(render_svg_image(SVG, 0, 8).is_none());
        assert!(render_svg_image(SVG, 12, 0).is_none());
        assert!(render_svg_image(SVG, -1, 8).is_none());
        assert!(render_svg_image(SVG, 12, -1).is_none());
    }

    #[test]
    fn rejects_gzip_compressed_svg_input() {
        const GZIP_COMPRESSED_SVG: &[u8] = &[
            31, 139, 8, 0, 0, 0, 0, 0, 0, 3, 179, 41, 46, 75, 87, 168, 200, 205, 201, 43, 182, 85,
            202, 40, 41, 41, 176, 210, 215, 47, 47, 47, 215, 43, 55, 214, 203, 47, 74, 215, 55, 50,
            48, 48, 208, 7, 170, 80, 82, 40, 207, 76, 41, 201, 176, 85, 50, 52, 82, 82, 200, 72,
            205, 76, 207, 40, 177, 85, 178, 80, 210, 183, 3, 0, 223, 158, 164, 196, 63, 0, 0, 0,
        ];

        assert!(!svg_intrinsic_size(GZIP_COMPRESSED_SVG).valid);
        assert!(render_svg_image(GZIP_COMPRESSED_SVG, 12, 8).is_none());
    }

    #[test]
    fn renders_translucent_pixels_as_premultiplied_rgba_bytes() {
        let bytes = render_svg_image(
            br##"<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1">
<rect width="1" height="1" fill="#c86432" fill-opacity="0.5"/>
</svg>"##,
            1,
            1,
        )
        .expect("translucent SVG should render");

        assert_eq!(bytes, [100, 50, 25, 128]);
    }

    #[test]
    fn does_not_load_external_file_resources() {
        let external = tempfile::Builder::new()
            .suffix(".png")
            .tempfile()
            .expect("external image fixture should be created");
        fs::write(external.path(), encode_rgba_png([255, 0, 0, 255]))
            .expect("external image fixture should be written");
        let svg = format!(
            r#"<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1">
<rect width="1" height="1" fill="blue"/>
<image href="file://{}" width="1" height="1"/>
</svg>"#,
            external.path().display()
        );

        let bytes = render_svg_image(svg.as_bytes(), 1, 1)
            .expect("SVG with a blocked external image should render");

        assert_eq!(bytes, [0, 0, 255, 255]);
    }

    fn encode_rgba_png(pixel: [u8; 4]) -> Vec<u8> {
        let mut bytes = Vec::new();
        let mut encoder = png::Encoder::new(&mut bytes, 1, 1);
        encoder.set_color(png::ColorType::Rgba);
        encoder.set_depth(png::BitDepth::Eight);
        encoder
            .write_header()
            .expect("PNG header should be written")
            .write_image_data(&pixel)
            .expect("PNG pixel should be written");

        bytes
    }
}
