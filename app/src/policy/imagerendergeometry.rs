// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#[cxx::bridge(namespace = "kiriview")]
mod ffi {
    #[derive(Clone, Copy, Debug, PartialEq)]
    struct RustImageRenderSize {
        width: i32,
        height: i32,
    }

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct RustImageRenderSizeF {
        width: f64,
        height: f64,
    }

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct RustImageRenderRectF {
        x: f64,
        y: f64,
        width: f64,
        height: f64,
    }

    extern "Rust" {
        #[cxx_name = "rustImageTargetRect"]
        fn rust_image_target_rect(
            image_size: RustImageRenderSize,
            bounds_size: RustImageRenderSizeF,
        ) -> RustImageRenderRectF;

        #[cxx_name = "rustScaledImageSizeToFit"]
        fn rust_scaled_image_size_to_fit(
            image_size: RustImageRenderSizeF,
            bounds_size: RustImageRenderSize,
        ) -> RustImageRenderSize;

        #[cxx_name = "rustFirstDisplayScaledImageSize"]
        fn rust_first_display_scaled_image_size(
            image_size: RustImageRenderSize,
            logical_viewport_size: RustImageRenderSize,
        ) -> RustImageRenderSize;

    }
}

use ffi::{RustImageRenderRectF, RustImageRenderSize, RustImageRenderSizeF};

fn rust_image_target_rect(
    image_size: RustImageRenderSize,
    bounds_size: RustImageRenderSizeF,
) -> RustImageRenderRectF {
    if size_empty(image_size) || size_f_empty(bounds_size) {
        return empty_rect_f();
    }

    let scale = min_like_cpp(
        bounds_size.width / f64::from(image_size.width),
        bounds_size.height / f64::from(image_size.height),
    );
    let target_width = f64::from(image_size.width) * scale;
    let target_height = f64::from(image_size.height) * scale;
    RustImageRenderRectF {
        x: (bounds_size.width - target_width) / 2.0,
        y: (bounds_size.height - target_height) / 2.0,
        width: target_width,
        height: target_height,
    }
}

fn rust_scaled_image_size_to_fit(
    image_size: RustImageRenderSizeF,
    bounds_size: RustImageRenderSize,
) -> RustImageRenderSize {
    if size_f_empty(image_size) || size_empty(bounds_size) {
        return empty_size();
    }

    let image_width = image_size.width;
    let image_height = image_size.height;
    if !image_width.is_finite()
        || !image_height.is_finite()
        || image_width <= 0.0
        || image_height <= 0.0
    {
        return empty_size();
    }

    let scale = min_like_cpp(
        1.0,
        min_like_cpp(
            f64::from(bounds_size.width) / image_width,
            f64::from(bounds_size.height) / image_height,
        ),
    );
    if !scale.is_finite() || scale <= 0.0 {
        return empty_size();
    }

    RustImageRenderSize {
        width: ceil_clamped_to_i32(image_width * scale, 1, bounds_size.width),
        height: ceil_clamped_to_i32(image_height * scale, 1, bounds_size.height),
    }
}

fn rust_first_display_scaled_image_size(
    image_size: RustImageRenderSize,
    logical_viewport_size: RustImageRenderSize,
) -> RustImageRenderSize {
    if size_empty(image_size) || size_empty(logical_viewport_size) {
        return empty_size();
    }

    let scaled_size = rust_scaled_image_size_to_fit(
        RustImageRenderSizeF {
            width: f64::from(image_size.width),
            height: f64::from(image_size.height),
        },
        logical_viewport_size,
    );
    if size_empty(scaled_size) || scaled_size == image_size {
        return empty_size();
    }

    scaled_size
}

fn min_like_cpp(left: f64, right: f64) -> f64 {
    if right < left { right } else { left }
}

fn ceil_clamped_to_i32(value: f64, minimum: i32, maximum: i32) -> i32 {
    let rounded = value.ceil();
    if rounded <= f64::from(minimum) {
        minimum
    } else if rounded >= f64::from(maximum) {
        maximum
    } else {
        rounded as i32
    }
}

fn size_empty(size: RustImageRenderSize) -> bool {
    size.width <= 0 || size.height <= 0
}

fn size_f_empty(size: RustImageRenderSizeF) -> bool {
    size.width <= 0.0 || size.height <= 0.0
}

fn empty_size() -> RustImageRenderSize {
    RustImageRenderSize {
        width: -1,
        height: -1,
    }
}

fn empty_rect_f() -> RustImageRenderRectF {
    RustImageRenderRectF {
        x: 0.0,
        y: 0.0,
        width: 0.0,
        height: 0.0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn size(width: i32, height: i32) -> RustImageRenderSize {
        RustImageRenderSize { width, height }
    }

    fn size_f(width: f64, height: f64) -> RustImageRenderSizeF {
        RustImageRenderSizeF { width, height }
    }

    #[test]
    fn target_rect_fits_and_centers_image_in_bounds() {
        let rect = rust_image_target_rect(size(1000, 500), size_f(400.0, 400.0));

        assert_eq!(rect.x, 0.0);
        assert_eq!(rect.y, 100.0);
        assert_eq!(rect.width, 400.0);
        assert_eq!(rect.height, 200.0);
    }

    #[test]
    fn scaled_size_preserves_aspect_ratio_without_upscaling() {
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(4000.0, 2000.0), size(1000, 1000)),
            size(1000, 500)
        );
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(333.0, 100.0), size(200, 200)),
            size(200, 61)
        );
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(200.0, 100.0), size(1000, 1000)),
            size(200, 100)
        );
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(0.5, 0.5), size(100, 100)),
            size(1, 1)
        );
    }

    #[test]
    fn scaled_size_rejects_invalid_input() {
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(0.0, 100.0), size(100, 100)),
            empty_size()
        );
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(f64::NAN, 100.0), size(100, 100)),
            empty_size()
        );
        assert_eq!(
            rust_scaled_image_size_to_fit(size_f(100.0, 100.0), size(0, 100)),
            empty_size()
        );
    }

    #[test]
    fn first_display_scaled_image_size_downscales_without_full_size_decode() {
        assert_eq!(
            rust_first_display_scaled_image_size(size(1600, 1200), size(400, 300)),
            size(400, 300)
        );
        assert_eq!(
            rust_first_display_scaled_image_size(size(200, 100), size(400, 300)),
            empty_size()
        );
        assert_eq!(
            rust_first_display_scaled_image_size(size(1600, 1200), size(0, 300)),
            empty_size()
        );
    }
}
