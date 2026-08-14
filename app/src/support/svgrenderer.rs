// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use resvg::{
    tiny_skia::{Pixmap, Transform},
    usvg::{Error as UsvgError, ImageHrefResolver, Options, Tree, roxmltree},
};
use std::borrow::Cow;

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
    #[derive(Debug)]
    enum RustSvgOperationStatus {
        Success = 0,
        DecodeError = 1,
        ResourceExhausted = 2,
    }

    #[namespace = "kiriview"]
    extern "Rust" {
        #[cxx_name = "rustSvgIntrinsicSize"]
        fn rust_svg_intrinsic_size(
            data: &[u8],
            status: &mut RustSvgOperationStatus,
        ) -> RustSvgImageSize;

        #[cxx_name = "rustRenderSvgImage"]
        fn rust_render_svg_image(
            data: &[u8],
            width: i32,
            height: i32,
            status: &mut RustSvgOperationStatus,
        ) -> Vec<u8>;

    }
}

use ffi::{RustSvgImageSize, RustSvgOperationStatus};

#[derive(Clone, Copy, Debug, PartialEq)]
enum SvgRenderFailure {
    Decode,
    ResourceExhausted,
}

fn rust_svg_intrinsic_size(data: &[u8], status: &mut RustSvgOperationStatus) -> RustSvgImageSize {
    match svg_intrinsic_size(data) {
        Ok(size) => {
            *status = RustSvgOperationStatus::Success;
            size
        }
        Err(failure) => {
            *status = svg_render_failure_status(failure);
            invalid_svg_image_size()
        }
    }
}

fn rust_render_svg_image(
    data: &[u8],
    width: i32,
    height: i32,
    status: &mut RustSvgOperationStatus,
) -> Vec<u8> {
    bridge_svg_render_result(render_svg_image(data, width, height), status)
}

fn bridge_svg_render_result(
    result: Result<Vec<u8>, SvgRenderFailure>,
    status: &mut RustSvgOperationStatus,
) -> Vec<u8> {
    match result {
        Ok(bytes) => {
            *status = RustSvgOperationStatus::Success;
            bytes
        }
        Err(failure) => {
            *status = svg_render_failure_status(failure);
            Vec::new()
        }
    }
}

fn svg_render_failure_status(failure: SvgRenderFailure) -> RustSvgOperationStatus {
    match failure {
        SvgRenderFailure::Decode => RustSvgOperationStatus::DecodeError,
        SvgRenderFailure::ResourceExhausted => RustSvgOperationStatus::ResourceExhausted,
    }
}

fn svg_intrinsic_size(data: &[u8]) -> Result<RustSvgImageSize, SvgRenderFailure> {
    let tree = parse_svg_tree(data)?;

    Ok(rust_svg_image_size(
        tree.size().to_int_size().width(),
        tree.size().to_int_size().height(),
    ))
}

fn render_svg_image(data: &[u8], width: i32, height: i32) -> Result<Vec<u8>, SvgRenderFailure> {
    let (width, height) = positive_dimensions(width, height).ok_or(SvgRenderFailure::Decode)?;
    let tree = parse_svg_tree(data)?;
    let transform = image_transform(&tree, width, height);

    render_tree(&tree, width, height, transform)
}

fn parse_svg_tree(data: &[u8]) -> Result<Tree, SvgRenderFailure> {
    if data.starts_with(&[0x1f, 0x8b]) {
        return Err(SvgRenderFailure::Decode);
    }

    // usvg decodes data URLs before invoking the configured resolver, so remove resource-bearing
    // image hrefs before parsing instead of relying on the resolver to reject them afterward.
    let data = svg_without_nested_image_resources(data);
    let options = svg_options();
    Tree::from_data(data.as_ref(), &options).map_err(classify_usvg_error)
}

fn classify_usvg_error(error: UsvgError) -> SvgRenderFailure {
    match error {
        UsvgError::ElementsLimitReached
        | UsvgError::ParsingFailed(
            roxmltree::Error::NodesLimitReached
            | roxmltree::Error::AttributesLimitReached
            | roxmltree::Error::NamespacesLimitReached,
        ) => SvgRenderFailure::ResourceExhausted,
        _ => SvgRenderFailure::Decode,
    }
}

fn svg_without_nested_image_resources(data: &[u8]) -> Cow<'_, [u8]> {
    let mut sanitized = None;
    let mut cursor = 0;
    while let Some(relative_start) = data[cursor..].iter().position(|byte| *byte == b'<') {
        let tag_start = cursor + relative_start;
        if data[tag_start..].starts_with(b"<!--") {
            cursor = end_of_xml_section(data, tag_start + 4, b"-->").unwrap_or(data.len());
            continue;
        }
        if data[tag_start..].starts_with(b"<![CDATA[") {
            cursor = end_of_xml_section(data, tag_start + 9, b"]]>").unwrap_or(data.len());
            continue;
        }

        let Some(tag_end) = xml_tag_end(data, tag_start + 1) else {
            break;
        };
        neutralize_nested_image_href(data, tag_start + 1, tag_end, &mut sanitized);
        cursor = tag_end + 1;
    }

    sanitized.map_or(Cow::Borrowed(data), Cow::Owned)
}

fn end_of_xml_section(data: &[u8], start: usize, terminator: &[u8]) -> Option<usize> {
    data.get(start..)?
        .windows(terminator.len())
        .position(|window| window == terminator)
        .map(|offset| start + offset + terminator.len())
}

fn xml_tag_end(data: &[u8], start: usize) -> Option<usize> {
    let mut quote = None;
    for (offset, byte) in data.get(start..)?.iter().copied().enumerate() {
        if matches!(byte, b'\'' | b'"') {
            if quote == Some(byte) {
                quote = None;
            } else if quote.is_none() {
                quote = Some(byte);
            }
        } else if byte == b'>' && quote.is_none() {
            return Some(start + offset);
        }
    }
    None
}

fn neutralize_nested_image_href(
    data: &[u8],
    tag_start: usize,
    tag_end: usize,
    sanitized: &mut Option<Vec<u8>>,
) {
    let mut cursor = tag_start;
    if data
        .get(cursor)
        .is_none_or(|byte| matches!(*byte, b'/' | b'!' | b'?'))
    {
        return;
    }
    let name_start = cursor;
    while cursor < tag_end && !is_xml_name_separator(data[cursor]) {
        cursor += 1;
    }
    let element_name = local_xml_name(&data[name_start..cursor]);
    if element_name != b"image" && element_name != b"feImage" {
        return;
    }

    while cursor < tag_end {
        while cursor < tag_end && (data[cursor].is_ascii_whitespace() || data[cursor] == b'/') {
            cursor += 1;
        }
        let attribute_start = cursor;
        while cursor < tag_end && !is_xml_name_separator(data[cursor]) && data[cursor] != b'=' {
            cursor += 1;
        }
        let attribute_end = cursor;
        while cursor < tag_end && data[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
        if cursor >= tag_end || data[cursor] != b'=' {
            cursor = cursor.saturating_add(1);
            continue;
        }
        cursor += 1;
        while cursor < tag_end && data[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
        if cursor >= tag_end || !matches!(data[cursor], b'\'' | b'"') {
            continue;
        }
        let quote = data[cursor];
        cursor += 1;
        let value_start = cursor;
        while cursor < tag_end && data[cursor] != quote {
            cursor += 1;
        }
        if local_xml_name(&data[attribute_start..attribute_end]) == b"href" && value_start < cursor
        {
            sanitized.get_or_insert_with(|| data.to_vec())[value_start] = b'x';
        }
        cursor = cursor.saturating_add(1);
    }
}

fn local_xml_name(name: &[u8]) -> &[u8] {
    name.rsplit(|byte| *byte == b':').next().unwrap_or(name)
}

fn is_xml_name_separator(byte: u8) -> bool {
    byte.is_ascii_whitespace() || matches!(byte, b'/' | b'>')
}

fn svg_options() -> Options<'static> {
    Options {
        resources_dir: None,
        image_href_resolver: ImageHrefResolver {
            resolve_data: Box::new(|_, _, _| None),
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

fn render_tree(
    tree: &Tree,
    width: u32,
    height: u32,
    transform: Transform,
) -> Result<Vec<u8>, SvgRenderFailure> {
    let mut pixmap = Pixmap::new(width, height).ok_or(SvgRenderFailure::ResourceExhausted)?;
    resvg::render(tree, transform, &mut pixmap.as_mut());
    Ok(pixmap.take())
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
        )
        .expect("valid SVG should have an intrinsic size");

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

        assert_eq!(
            svg_intrinsic_size(b"not svg"),
            Err(SvgRenderFailure::Decode)
        );
        assert_eq!(
            render_svg_image(b"not svg", 12, 8),
            Err(SvgRenderFailure::Decode)
        );
        assert_eq!(render_svg_image(SVG, 0, 8), Err(SvgRenderFailure::Decode));
        assert_eq!(render_svg_image(SVG, 12, 0), Err(SvgRenderFailure::Decode));
        assert_eq!(render_svg_image(SVG, -1, 8), Err(SvgRenderFailure::Decode));
        assert_eq!(render_svg_image(SVG, 12, -1), Err(SvgRenderFailure::Decode));
    }

    #[test]
    fn rejects_gzip_compressed_svg_input() {
        const GZIP_COMPRESSED_SVG: &[u8] = &[
            31, 139, 8, 0, 0, 0, 0, 0, 0, 3, 179, 41, 46, 75, 87, 168, 200, 205, 201, 43, 182, 85,
            202, 40, 41, 41, 176, 210, 215, 47, 47, 47, 215, 43, 55, 214, 203, 47, 74, 215, 55, 50,
            48, 48, 208, 7, 170, 80, 82, 40, 207, 76, 41, 201, 176, 85, 50, 52, 82, 82, 200, 72,
            205, 76, 207, 40, 177, 85, 178, 80, 210, 183, 3, 0, 223, 158, 164, 196, 63, 0, 0, 0,
        ];

        assert_eq!(
            svg_intrinsic_size(GZIP_COMPRESSED_SVG),
            Err(SvgRenderFailure::Decode)
        );
        assert_eq!(
            render_svg_image(GZIP_COMPRESSED_SVG, 12, 8),
            Err(SvgRenderFailure::Decode)
        );
    }

    #[test]
    fn bridge_preserves_decode_and_resource_failure_kinds() {
        let mut status = RustSvgOperationStatus::Success;
        assert!(bridge_svg_render_result(Err(SvgRenderFailure::Decode), &mut status).is_empty());
        assert_eq!(status, RustSvgOperationStatus::DecodeError);

        status = RustSvgOperationStatus::Success;
        assert!(
            bridge_svg_render_result(Err(SvgRenderFailure::ResourceExhausted), &mut status)
                .is_empty()
        );
        assert_eq!(status, RustSvgOperationStatus::ResourceExhausted);
    }

    #[test]
    fn parser_security_limits_are_resource_failures() {
        assert_eq!(
            classify_usvg_error(UsvgError::ElementsLimitReached),
            SvgRenderFailure::ResourceExhausted
        );
        assert_eq!(
            classify_usvg_error(UsvgError::ParsingFailed(
                roxmltree::Error::NodesLimitReached
            )),
            SvgRenderFailure::ResourceExhausted
        );
        assert_eq!(
            classify_usvg_error(UsvgError::InvalidSize),
            SvgRenderFailure::Decode
        );
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

    #[test]
    fn does_not_load_embedded_data_resources() {
        let bytes = render_svg_image(
            br##"<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1">
<rect width="1" height="1" fill="blue"/>
<image href="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxIiBoZWlnaHQ9IjEiPjxyZWN0IHdpZHRoPSIxIiBoZWlnaHQ9IjEiIGZpbGw9InJlZCIvPjwvc3ZnPg==" width="1" height="1"/>
</svg>"##,
            1,
            1,
        )
        .expect("SVG with a blocked embedded image should render");

        assert_eq!(bytes, [0, 0, 255, 255]);
    }

    #[test]
    fn removes_image_resources_before_parsing_without_removing_internal_references() {
        let source = br##"<svg xmlns="http://www.w3.org/2000/svg">
<image href="&#x64;ata:image/png;base64,AAAA"/>
<use href="#shape"/>
</svg>"##;

        let sanitized = svg_without_nested_image_resources(source);

        assert_ne!(sanitized.as_ref(), source);
        assert!(
            !sanitized
                .windows(b"&#x64;ata:".len())
                .any(|window| window == b"&#x64;ata:")
        );
        assert!(
            sanitized
                .windows(b"href=\"#shape\"".len())
                .any(|window| window == b"href=\"#shape\"")
        );
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
