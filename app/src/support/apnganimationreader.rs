// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use std::io::Cursor;

use png::{
    BitDepth, BlendOp, ColorType, Decoder, DecodingError, DisposeOp, FrameControl, Limits, Reader,
    Transformations,
};

#[cxx::bridge(namespace = "kiriview")]
mod ffi {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustApngOpenStatus {
        NotApng = 0,
        Success = 1,
        Error = 2,
        ResourceLimitExceeded = 3,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustApngProbeStatus {
        NotApng = 0,
        Success = 1,
        Error = 2,
        ResourceLimitExceeded = 3,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustApngReadStatus {
        Frame = 0,
        End = 1,
        Error = 2,
        ResourceLimitExceeded = 3,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustApngDisposeOp {
        None = 0,
        Background = 1,
        Previous = 2,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustApngBlendOp {
        Source = 0,
        Over = 1,
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct RustApngOpenResult {
        status: RustApngOpenStatus,
        canvas_width: u32,
        canvas_height: u32,
        loop_count: i32,
        frame_count: i32,
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct RustApngProbeResult {
        status: RustApngProbeStatus,
        canvas_width: u32,
        canvas_height: u32,
        decoder_workspace_byte_count: usize,
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct RustApngFrameResult {
        status: RustApngReadStatus,
        width: u32,
        height: u32,
        x_offset: u32,
        y_offset: u32,
        delay_num: u32,
        delay_den: u32,
        dispose_op: RustApngDisposeOp,
        blend_op: RustApngBlendOp,
        row_bytes: usize,
        has_more_frames: bool,
        pixels: Vec<u8>,
    }

    extern "Rust" {
        type RustApngAnimationReader;

        #[cxx_name = "rustNewApngAnimationReader"]
        fn rust_new_apng_animation_reader() -> Box<RustApngAnimationReader>;

        #[cxx_name = "rustProbeApngAnimation"]
        fn rust_probe_apng_animation(data: &[u8], max_decoder_bytes: usize) -> RustApngProbeResult;

        #[cxx_name = "rustOpenApngAnimationReader"]
        fn rust_open_apng_animation_reader(
            reader: &mut RustApngAnimationReader,
            data: &[u8],
            max_decoder_bytes: usize,
        ) -> RustApngOpenResult;

        #[cxx_name = "rustReadApngAnimationFrame"]
        fn rust_read_apng_animation_frame(
            reader: &mut RustApngAnimationReader,
            max_output_bytes: usize,
        ) -> RustApngFrameResult;

        #[cxx_name = "rustApngAnimationReaderHasMoreFrames"]
        fn rust_apng_animation_reader_has_more_frames(reader: &RustApngAnimationReader) -> bool;
    }
}

use ffi::{
    RustApngBlendOp, RustApngDisposeOp, RustApngFrameResult, RustApngOpenResult,
    RustApngOpenStatus, RustApngProbeResult, RustApngProbeStatus, RustApngReadStatus,
};

const PNG_SIGNATURE: &[u8] = b"\x89PNG\r\n\x1a\n";
#[cfg(test)]
const PNG_DECODER_BYTE_LIMIT: usize = 64 * 1024 * 1024;
const DECODER_FIXED_HEADROOM_BYTES: u64 = 1024 * 1024;
const UNFILTER_GROWTH_BYTES: u64 = 8 * 1024;
const DEFLATE_LOOKBACK_BYTES: u64 = 32 * 1024;
const MINIMUM_UNFILTER_CAPACITY_BYTES: u64 = 128 * 1024;
const INITIAL_CHUNK_CAPACITY_BYTES: u64 = 128;

struct ApngProbeImageHeader {
    width: u32,
    height: u32,
    source_samples: u64,
    source_bit_depth: u64,
    output_samples: u64,
    interlaced: bool,
}

type PngReader = Reader<Cursor<Vec<u8>>>;

struct PngChunk<'a> {
    kind: &'a [u8],
    payload: &'a [u8],
    next_offset: usize,
}

fn be_u32(bytes: &[u8]) -> Option<u32> {
    Some(u32::from_be_bytes(bytes.get(..4)?.try_into().ok()?))
}

fn png_chunk_at(data: &[u8], offset: usize) -> Option<PngChunk<'_>> {
    let header_end = offset.checked_add(8)?;
    let header = data.get(offset..header_end)?;
    let payload_size = usize::try_from(be_u32(header)?).ok()?;
    if payload_size > 0x7fff_ffff {
        return None;
    }
    let payload_end = header_end.checked_add(payload_size)?;
    let next_offset = payload_end.checked_add(4)?;
    let payload = data.get(header_end..payload_end)?;
    data.get(payload_end..next_offset)?;
    Some(PngChunk {
        kind: &header[4..8],
        payload,
        next_offset,
    })
}

fn probe_image_header(payload: &[u8]) -> Option<ApngProbeImageHeader> {
    if payload.len() != 13 || payload[10] != 0 || payload[11] != 0 {
        return None;
    }
    let width = be_u32(payload)?;
    let height = be_u32(&payload[4..])?;
    if width == 0 || height == 0 {
        return None;
    }

    let source_bit_depth = u64::from(payload[8]);
    let (source_samples, output_samples, valid_depths): (u64, u64, &[u8]) = match payload[9] {
        0 => (1, 2, &[1, 2, 4, 8, 16]),
        2 => (3, 4, &[8, 16]),
        3 => (1, 4, &[1, 2, 4, 8]),
        4 => (2, 2, &[8, 16]),
        6 => (4, 4, &[8, 16]),
        _ => return None,
    };
    if !valid_depths.contains(&payload[8]) {
        return None;
    }
    let interlaced = match payload[12] {
        0 => false,
        1 => true,
        _ => return None,
    };

    Some(ApngProbeImageHeader {
        width,
        height,
        source_samples,
        source_bit_depth,
        output_samples,
        interlaced,
    })
}

fn checked_round_up(value: u64, alignment: u64) -> Option<u64> {
    let remainder = value % alignment;
    if remainder == 0 {
        Some(value)
    } else {
        value.checked_add(alignment - remainder)
    }
}

fn checked_raw_row_byte_count(header: &ApngProbeImageHeader, width: u64) -> Option<u64> {
    let bit_count = width
        .checked_mul(header.source_samples)?
        .checked_mul(header.source_bit_depth)?;
    1_u64.checked_add(bit_count.checked_add(7)? / 8)
}

fn checked_decoder_workspace_byte_count(
    header: &ApngProbeImageHeader,
    input_byte_count: usize,
    decoder_byte_limit: usize,
) -> Option<usize> {
    // This mirrors the requested-capacity peaks of png 0.18.1 and the Rust Vec growth policy used
    // by the locked toolchain. Keep the formula regression tests aligned with dependency updates.
    let width = u64::from(header.width);
    let height = u64::from(header.height);
    let input_byte_count = u64::try_from(input_byte_count).ok()?;
    let decoder_byte_limit = u64::try_from(decoder_byte_limit).ok()?;
    let output_row_byte_count = width.checked_mul(header.output_samples)?;
    if output_row_byte_count > decoder_byte_limit {
        return None;
    }

    let full_raw_row_byte_count = checked_raw_row_byte_count(header, width)?;
    let (maximum_raw_row_byte_count, raw_frame_byte_count) = if header.interlaced {
        const ADAM7_PASSES: [(u64, u64, u64, u64); 7] = [
            (8, 0, 8, 0),
            (8, 4, 8, 0),
            (4, 0, 8, 4),
            (4, 2, 4, 0),
            (2, 0, 4, 2),
            (2, 1, 2, 0),
            (1, 0, 2, 1),
        ];
        let mut maximum_row = 0_u64;
        let mut frame_bytes = 0_u64;
        for (x_sampling, x_offset, y_sampling, y_offset) in ADAM7_PASSES {
            let pass_width = width.saturating_sub(x_offset).div_ceil(x_sampling);
            let pass_height = height.saturating_sub(y_offset).div_ceil(y_sampling);
            if pass_width == 0 || pass_height == 0 {
                continue;
            }
            let row_bytes = checked_raw_row_byte_count(header, pass_width)?;
            maximum_row = maximum_row.max(row_bytes);
            frame_bytes = frame_bytes.checked_add(row_bytes.checked_mul(pass_height)?)?;
        }
        (maximum_row, frame_bytes)
    } else {
        (
            full_raw_row_byte_count,
            full_raw_row_byte_count.checked_mul(height)?,
        )
    };

    let initial_unfilter_capacity =
        checked_round_up(full_raw_row_byte_count.checked_mul(height.min(128))?, 256)?
            .min(MINIMUM_UNFILTER_CAPACITY_BYTES);
    let shift_back_limit = checked_round_up(full_raw_row_byte_count.checked_mul(4)?, 64)?
        .max(MINIMUM_UNFILTER_CAPACITY_BYTES);
    let stream_window = shift_back_limit
        .checked_add(maximum_raw_row_byte_count.checked_mul(2)?)?
        .checked_add(DEFLATE_LOOKBACK_BYTES)?;
    let data_stream_request =
        UNFILTER_GROWTH_BYTES.checked_add(raw_frame_byte_count.min(stream_window))?;
    let stable_data_stream_capacity =
        initial_unfilter_capacity.max(data_stream_request.checked_mul(2)?);
    let peak_data_stream_reallocation =
        initial_unfilter_capacity.max(data_stream_request.checked_mul(3)?);

    let raw_pixel_row_byte_count = maximum_raw_row_byte_count.checked_sub(1)?;
    let stable_scratch_capacity = 8_u64.max(raw_pixel_row_byte_count.checked_mul(2)?);
    let stable_scratch_bytes = stable_scratch_capacity.checked_mul(2)?;
    let peak_scratch_reallocation = 8_u64
        .max(raw_pixel_row_byte_count.checked_mul(3)?)
        .checked_add(stable_scratch_capacity)?;
    let stable_unfilter_bytes = stable_data_stream_capacity.checked_add(stable_scratch_bytes)?;
    let peak_unfilter_bytes = peak_data_stream_reallocation
        .checked_add(stable_scratch_bytes)?
        .max(stable_data_stream_capacity.checked_add(peak_scratch_reallocation)?);
    let output_scratch_byte_count = if header.interlaced {
        8_u64.max(output_row_byte_count)
    } else {
        0
    };

    let chunk_capacity = INITIAL_CHUNK_CAPACITY_BYTES.max(
        INITIAL_CHUNK_CAPACITY_BYTES
            .checked_add(decoder_byte_limit)?
            .min(input_byte_count.checked_mul(2)?),
    );
    let retained_exif_byte_count = input_byte_count.min(chunk_capacity);
    let frame_peak = retained_exif_byte_count
        .checked_add(chunk_capacity)?
        .checked_add(peak_unfilter_bytes)?
        .checked_add(output_scratch_byte_count)?;
    let later_chunk_peak = retained_exif_byte_count
        .checked_add(chunk_capacity.checked_mul(2)?)?
        .checked_add(stable_unfilter_bytes)?
        .checked_add(output_scratch_byte_count)?;
    usize::try_from(
        frame_peak
            .max(later_chunk_peak)
            .checked_add(DECODER_FIXED_HEADROOM_BYTES)?,
    )
    .ok()
}

fn rust_probe_apng_animation(data: &[u8], max_decoder_bytes: usize) -> RustApngProbeResult {
    // This is an allocation-free admission probe, not an authoritative PNG validator. A
    // successful candidate is still opened and validated by the png decoder after admission.
    if !data.starts_with(PNG_SIGNATURE) {
        return probe_not_apng();
    }

    let Some(header) = png_chunk_at(data, PNG_SIGNATURE.len()) else {
        return probe_error();
    };
    if header.kind != b"IHDR" {
        return probe_error();
    }
    let Some(image_header) = probe_image_header(header.payload) else {
        return probe_error();
    };

    let mut offset = header.next_offset;
    loop {
        let Some(chunk) = png_chunk_at(data, offset) else {
            return probe_error();
        };
        match chunk.kind {
            b"acTL" => {
                if chunk.payload.len() != 8 || be_u32(chunk.payload) == Some(0) {
                    return probe_error();
                }
                let Some(decoder_workspace_byte_count) = checked_decoder_workspace_byte_count(
                    &image_header,
                    data.len(),
                    max_decoder_bytes,
                ) else {
                    return probe_resource_limit_exceeded();
                };
                return RustApngProbeResult {
                    status: RustApngProbeStatus::Success,
                    canvas_width: image_header.width,
                    canvas_height: image_header.height,
                    decoder_workspace_byte_count,
                };
            }
            b"IDAT" | b"IEND" => return probe_not_apng(),
            b"fdAT" => return probe_error(),
            _ => offset = chunk.next_offset,
        }
    }
}

pub(crate) struct RustApngAnimationReader {
    reader: Option<PngReader>,
    raw_frames_remaining: u32,
}

fn rust_new_apng_animation_reader() -> Box<RustApngAnimationReader> {
    Box::new(RustApngAnimationReader {
        reader: None,
        raw_frames_remaining: 0,
    })
}

fn rust_open_apng_animation_reader(
    state: &mut RustApngAnimationReader,
    data: &[u8],
    max_decoder_bytes: usize,
) -> RustApngOpenResult {
    state.reader = None;
    state.raw_frames_remaining = 0;

    match rust_probe_apng_animation(data, max_decoder_bytes).status {
        RustApngProbeStatus::NotApng => return open_not_apng(),
        RustApngProbeStatus::Error => return open_error(),
        RustApngProbeStatus::ResourceLimitExceeded => return open_resource_limit_exceeded(),
        RustApngProbeStatus::Success => {}
        _ => return open_error(),
    }

    let mut owned_data = Vec::new();
    if owned_data.try_reserve_exact(data.len()).is_err() {
        return open_resource_limit_exceeded();
    }
    owned_data.extend_from_slice(data);

    let mut decoder = Decoder::new_with_limits(
        Cursor::new(owned_data),
        Limits {
            bytes: max_decoder_bytes,
        },
    );
    decoder.set_transformations(
        Transformations::EXPAND | Transformations::STRIP_16 | Transformations::ALPHA,
    );
    decoder.set_ignore_text_chunk(true);
    decoder.set_ignore_iccp_chunk(true);

    let reader = match decoder.read_info() {
        Ok(reader) => reader,
        Err(DecodingError::LimitsExceeded) => return open_resource_limit_exceeded(),
        Err(_) => return open_error(),
    };

    let info = reader.info();
    let Some(animation) = info.animation_control else {
        return RustApngOpenResult {
            status: RustApngOpenStatus::NotApng,
            ..open_result_defaults()
        };
    };

    if animation.num_frames == 0 {
        return open_error();
    }

    let frame_count = match i32::try_from(animation.num_frames) {
        Ok(frame_count) if frame_count > 0 => frame_count,
        _ => return open_error(),
    };
    let Some(raw_frames_remaining) = animation
        .num_frames
        .checked_add(u32::from(info.frame_control.is_none()))
    else {
        return open_error();
    };

    if !is_rgba8_output(&reader) || info.width == 0 || info.height == 0 {
        return open_error();
    }

    let canvas_width = info.width;
    let canvas_height = info.height;
    state.raw_frames_remaining = raw_frames_remaining;
    state.reader = Some(reader);
    RustApngOpenResult {
        status: RustApngOpenStatus::Success,
        canvas_width,
        canvas_height,
        loop_count: apng_loop_count_for_play_count(animation.num_plays),
        frame_count,
    }
}

fn rust_read_apng_animation_frame(
    state: &mut RustApngAnimationReader,
    max_output_bytes: usize,
) -> RustApngFrameResult {
    let Some(reader) = state.reader.as_mut() else {
        return read_end();
    };

    loop {
        if state.raw_frames_remaining == 0 {
            state.reader = None;
            return read_end();
        }

        let Some(buffer_size) = reader.output_buffer_size() else {
            state.reader = None;
            return read_resource_limit_exceeded();
        };
        if buffer_size > max_output_bytes {
            state.reader = None;
            return read_resource_limit_exceeded();
        }

        let mut pixels = Vec::new();
        if pixels.try_reserve_exact(buffer_size).is_err() {
            state.reader = None;
            return read_resource_limit_exceeded();
        }
        pixels.resize(buffer_size, 0);
        let output = match reader.next_frame(&mut pixels) {
            Ok(output) => output,
            Err(DecodingError::LimitsExceeded) => {
                state.reader = None;
                return read_resource_limit_exceeded();
            }
            Err(_) => {
                state.reader = None;
                return read_error();
            }
        };
        state.raw_frames_remaining = state.raw_frames_remaining.saturating_sub(1);

        if !is_rgba8_output(reader) {
            state.reader = None;
            return read_error();
        }

        let Some(control) = reader.info().frame_control else {
            continue;
        };

        pixels.truncate(output.buffer_size());
        return frame_result(
            control,
            output.line_size,
            state.raw_frames_remaining > 0,
            pixels,
        );
    }
}

fn rust_apng_animation_reader_has_more_frames(state: &RustApngAnimationReader) -> bool {
    state.reader.is_some() && state.raw_frames_remaining > 0
}

fn is_rgba8_output(reader: &PngReader) -> bool {
    reader.output_color_type() == (ColorType::Rgba, BitDepth::Eight)
}

fn apng_loop_count_for_play_count(play_count: u32) -> i32 {
    if play_count == 0 {
        -1
    } else {
        i32::try_from(play_count.saturating_sub(1)).unwrap_or(i32::MAX)
    }
}

fn dispose_op(op: DisposeOp) -> RustApngDisposeOp {
    match op {
        DisposeOp::Background => RustApngDisposeOp::Background,
        DisposeOp::Previous => RustApngDisposeOp::Previous,
        DisposeOp::None => RustApngDisposeOp::None,
    }
}

fn blend_op(op: BlendOp) -> RustApngBlendOp {
    match op {
        BlendOp::Over => RustApngBlendOp::Over,
        BlendOp::Source => RustApngBlendOp::Source,
    }
}

fn frame_result(
    control: FrameControl,
    row_bytes: usize,
    has_more_frames: bool,
    pixels: Vec<u8>,
) -> RustApngFrameResult {
    RustApngFrameResult {
        status: RustApngReadStatus::Frame,
        width: control.width,
        height: control.height,
        x_offset: control.x_offset,
        y_offset: control.y_offset,
        delay_num: u32::from(control.delay_num),
        delay_den: u32::from(control.delay_den),
        dispose_op: dispose_op(control.dispose_op),
        blend_op: blend_op(control.blend_op),
        row_bytes,
        has_more_frames,
        pixels,
    }
}

fn open_result_defaults() -> RustApngOpenResult {
    RustApngOpenResult {
        status: RustApngOpenStatus::NotApng,
        canvas_width: 0,
        canvas_height: 0,
        loop_count: 0,
        frame_count: 0,
    }
}

fn open_not_apng() -> RustApngOpenResult {
    RustApngOpenResult {
        status: RustApngOpenStatus::NotApng,
        ..open_result_defaults()
    }
}

fn open_error() -> RustApngOpenResult {
    RustApngOpenResult {
        status: RustApngOpenStatus::Error,
        ..open_result_defaults()
    }
}

fn open_resource_limit_exceeded() -> RustApngOpenResult {
    RustApngOpenResult {
        status: RustApngOpenStatus::ResourceLimitExceeded,
        ..open_result_defaults()
    }
}

fn probe_not_apng() -> RustApngProbeResult {
    RustApngProbeResult {
        status: RustApngProbeStatus::NotApng,
        canvas_width: 0,
        canvas_height: 0,
        decoder_workspace_byte_count: 0,
    }
}

fn probe_error() -> RustApngProbeResult {
    RustApngProbeResult {
        status: RustApngProbeStatus::Error,
        canvas_width: 0,
        canvas_height: 0,
        decoder_workspace_byte_count: 0,
    }
}

fn probe_resource_limit_exceeded() -> RustApngProbeResult {
    RustApngProbeResult {
        status: RustApngProbeStatus::ResourceLimitExceeded,
        canvas_width: 0,
        canvas_height: 0,
        decoder_workspace_byte_count: 0,
    }
}

fn read_end() -> RustApngFrameResult {
    RustApngFrameResult {
        status: RustApngReadStatus::End,
        ..read_result_defaults()
    }
}

fn read_error() -> RustApngFrameResult {
    RustApngFrameResult {
        status: RustApngReadStatus::Error,
        ..read_result_defaults()
    }
}

fn read_resource_limit_exceeded() -> RustApngFrameResult {
    RustApngFrameResult {
        status: RustApngReadStatus::ResourceLimitExceeded,
        ..read_result_defaults()
    }
}

fn read_result_defaults() -> RustApngFrameResult {
    RustApngFrameResult {
        status: RustApngReadStatus::End,
        width: 0,
        height: 0,
        x_offset: 0,
        y_offset: 0,
        delay_num: 0,
        delay_den: 0,
        dispose_op: RustApngDisposeOp::None,
        blend_op: RustApngBlendOp::Source,
        row_bytes: 0,
        has_more_frames: false,
        pixels: Vec::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use png::{BitDepth, BlendOp, ColorType, DisposeOp, Encoder};

    const RED: &[u8] = &[255, 0, 0, 255];
    const BLUE: &[u8] = &[0, 0, 255, 255];
    const GREEN: &[u8] = &[0, 255, 0, 255];
    const TRANSLUCENT_BLUE: &[u8] = &[0, 0, 255, 128];

    #[derive(Clone)]
    struct TestFrame {
        width: u32,
        height: u32,
        x_offset: u32,
        y_offset: u32,
        delay_num: u16,
        delay_den: u16,
        dispose_op: DisposeOp,
        blend_op: BlendOp,
        pixels: Vec<u8>,
    }

    impl TestFrame {
        fn full_canvas(pixel: &[u8]) -> Self {
            Self {
                width: 1,
                height: 1,
                x_offset: 0,
                y_offset: 0,
                delay_num: 1,
                delay_den: 10,
                dispose_op: DisposeOp::None,
                blend_op: BlendOp::Source,
                pixels: pixel.to_vec(),
            }
        }
    }

    fn encode_plain_png(pixel: &[u8]) -> Vec<u8> {
        let mut output = Vec::new();
        let mut encoder = Encoder::new(&mut output, 1, 1);
        encoder.set_color(ColorType::Rgba);
        encoder.set_depth(BitDepth::Eight);
        encoder
            .write_header()
            .unwrap()
            .write_image_data(pixel)
            .unwrap();
        output
    }

    fn encode_apng(
        frames: &[TestFrame],
        play_count: u32,
        hidden_default: Option<&[u8]>,
    ) -> Vec<u8> {
        let mut output = Vec::new();
        let mut encoder = Encoder::new(&mut output, 1, 1);
        encoder.set_color(ColorType::Rgba);
        encoder.set_depth(BitDepth::Eight);
        encoder
            .set_animated(u32::try_from(frames.len()).unwrap(), play_count)
            .unwrap();
        encoder.set_sep_def_img(hidden_default.is_some()).unwrap();

        let mut writer = encoder.write_header().unwrap();
        if let Some(default_pixels) = hidden_default {
            writer.write_image_data(default_pixels).unwrap();
        }

        for frame in frames {
            writer
                .set_frame_dimension(frame.width, frame.height)
                .unwrap();
            writer
                .set_frame_position(frame.x_offset, frame.y_offset)
                .unwrap();
            writer
                .set_frame_delay(frame.delay_num, frame.delay_den)
                .unwrap();
            writer.set_dispose_op(frame.dispose_op).unwrap();
            writer.set_blend_op(frame.blend_op).unwrap();
            writer.write_image_data(&frame.pixels).unwrap();
        }

        writer.finish().unwrap();
        output
    }

    fn open(data: &[u8]) -> (Box<RustApngAnimationReader>, RustApngOpenResult) {
        let mut reader = rust_new_apng_animation_reader();
        let result = rust_open_apng_animation_reader(&mut reader, data, PNG_DECODER_BYTE_LIMIT);
        (reader, result)
    }

    #[test]
    fn non_png_and_plain_png_return_not_apng() {
        let (_, result) = open(b"not png");
        assert_eq!(result.status, RustApngOpenStatus::NotApng);

        let png = encode_plain_png(RED);
        let (_, result) = open(&png);
        assert_eq!(result.status, RustApngOpenStatus::NotApng);
    }

    #[test]
    fn sequential_frames_and_loop_count_are_reported() {
        let mut second = TestFrame::full_canvas(BLUE);
        second.delay_num = 2;
        let apng = encode_apng(&[TestFrame::full_canvas(RED), second], 2, None);

        let (mut reader, result) = open(&apng);
        assert_eq!(result.status, RustApngOpenStatus::Success);
        assert_eq!(result.canvas_width, 1);
        assert_eq!(result.canvas_height, 1);
        assert_eq!(result.frame_count, 2);
        assert_eq!(result.loop_count, 1);

        let first = rust_read_apng_animation_frame(&mut reader, usize::MAX);
        assert_eq!(first.status, RustApngReadStatus::Frame);
        assert_eq!(first.delay_num, 1);
        assert_eq!(first.delay_den, 10);
        assert_eq!(&first.pixels, RED);
        assert!(first.has_more_frames);

        let second = rust_read_apng_animation_frame(&mut reader, usize::MAX);
        assert_eq!(second.status, RustApngReadStatus::Frame);
        assert_eq!(second.delay_num, 2);
        assert_eq!(second.delay_den, 10);
        assert_eq!(&second.pixels, BLUE);
        assert!(!second.has_more_frames);

        assert_eq!(
            rust_read_apng_animation_frame(&mut reader, usize::MAX).status,
            RustApngReadStatus::End
        );
    }

    #[test]
    fn separate_default_image_is_skipped() {
        let hidden_default = [255, 0, 0, 255];
        let frame = TestFrame::full_canvas(BLUE);
        let apng = encode_apng(&[frame], 0, Some(&hidden_default));

        let (mut reader, result) = open(&apng);
        assert_eq!(result.status, RustApngOpenStatus::Success);
        assert_eq!(result.loop_count, -1);

        let frame = rust_read_apng_animation_frame(&mut reader, usize::MAX);
        assert_eq!(frame.status, RustApngReadStatus::Frame);
        assert_eq!(&frame.pixels, BLUE);
        assert!(!frame.has_more_frames);
    }

    #[test]
    fn frame_control_operations_are_reported() {
        let mut over = TestFrame::full_canvas(TRANSLUCENT_BLUE);
        over.blend_op = BlendOp::Over;
        let mut background = TestFrame::full_canvas(GREEN);
        background.dispose_op = DisposeOp::Background;
        let mut previous = TestFrame::full_canvas(RED);
        previous.dispose_op = DisposeOp::Previous;
        previous.delay_den = 0;
        let apng = encode_apng(&[over, background, previous], 1, None);

        let (mut reader, result) = open(&apng);
        assert_eq!(result.status, RustApngOpenStatus::Success);

        let over = rust_read_apng_animation_frame(&mut reader, usize::MAX);
        assert_eq!(over.blend_op, RustApngBlendOp::Over);

        let background = rust_read_apng_animation_frame(&mut reader, usize::MAX);
        assert_eq!(background.dispose_op, RustApngDisposeOp::Background);

        let previous = rust_read_apng_animation_frame(&mut reader, usize::MAX);
        assert_eq!(previous.dispose_op, RustApngDisposeOp::Previous);
        assert_eq!(previous.delay_num, 1);
        assert_eq!(previous.delay_den, 0);
    }

    #[test]
    fn malformed_apng_returns_error() {
        let apng = encode_apng(&[TestFrame::full_canvas(RED)], 1, None);
        let truncated = &apng[..apng.len() - 12];

        let (mut reader, result) = open(truncated);
        if result.status == RustApngOpenStatus::Success {
            assert_eq!(
                rust_read_apng_animation_frame(&mut reader, usize::MAX).status,
                RustApngReadStatus::Error
            );
        } else {
            assert_eq!(result.status, RustApngOpenStatus::Error);
        }
    }

    #[test]
    fn frame_output_limit_is_checked_before_allocation() {
        let apng = encode_apng(&[TestFrame::full_canvas(RED)], 1, None);
        let (mut reader, result) = open(&apng);
        assert_eq!(result.status, RustApngOpenStatus::Success);

        let frame = rust_read_apng_animation_frame(&mut reader, 3);
        assert_eq!(frame.status, RustApngReadStatus::ResourceLimitExceeded);
        assert!(frame.pixels.is_empty());
    }

    #[test]
    fn decoder_limit_failure_is_typed_during_open() {
        let apng = encode_apng(&[TestFrame::full_canvas(RED)], 1, None);
        let mut reader = rust_new_apng_animation_reader();

        let result = rust_open_apng_animation_reader(&mut reader, &apng, 0);

        assert_eq!(result.status, RustApngOpenStatus::ResourceLimitExceeded);
        assert!(!rust_apng_animation_reader_has_more_frames(&reader));
    }

    #[test]
    fn decoder_limit_failure_is_typed_during_later_frame_read() {
        let apng = encode_apng(
            &[TestFrame::full_canvas(RED), TestFrame::full_canvas(BLUE)],
            1,
            None,
        );
        let mut reader = rust_new_apng_animation_reader();
        let opened = rust_open_apng_animation_reader(&mut reader, &apng, 4);
        assert_eq!(opened.status, RustApngOpenStatus::Success);

        assert_eq!(
            rust_read_apng_animation_frame(&mut reader, usize::MAX).status,
            RustApngReadStatus::Frame
        );
        assert_eq!(
            rust_read_apng_animation_frame(&mut reader, usize::MAX).status,
            RustApngReadStatus::ResourceLimitExceeded
        );
    }

    #[test]
    fn probe_rejects_truncated_and_oversized_chunks() {
        let mut oversized_chunk = PNG_SIGNATURE.to_vec();
        oversized_chunk.extend_from_slice(&0x8000_0000_u32.to_be_bytes());
        oversized_chunk.extend_from_slice(b"IHDR");

        assert_eq!(
            rust_probe_apng_animation(PNG_SIGNATURE, PNG_DECODER_BYTE_LIMIT).status,
            RustApngProbeStatus::Error
        );
        assert_eq!(
            rust_probe_apng_animation(&oversized_chunk, PNG_DECODER_BYTE_LIMIT).status,
            RustApngProbeStatus::Error
        );
    }

    #[test]
    fn probe_reports_animation_header_before_image_data() {
        let apng = encode_apng(&[TestFrame::full_canvas(RED)], 1, None);
        let image_data_kind = apng
            .windows(4)
            .position(|window| window == b"IDAT")
            .unwrap();
        let candidate = &apng[..image_data_kind - 4];

        let result = rust_probe_apng_animation(candidate, PNG_DECODER_BYTE_LIMIT);

        assert_eq!(result.status, RustApngProbeStatus::Success);
        assert_eq!(result.canvas_width, 1);
        assert_eq!(result.canvas_height, 1);
        assert!(result.decoder_workspace_byte_count > DECODER_FIXED_HEADROOM_BYTES as usize);
        assert_eq!(
            rust_probe_apng_animation(&encode_plain_png(RED), PNG_DECODER_BYTE_LIMIT).status,
            RustApngProbeStatus::NotApng
        );
    }

    #[test]
    fn decoder_workspace_model_tracks_locked_png_buffer_growth() {
        let small_rgba8 = ApngProbeImageHeader {
            width: 1,
            height: 1,
            source_samples: 4,
            source_bit_depth: 8,
            output_samples: 4,
            interlaced: false,
        };
        assert_eq!(
            checked_decoder_workspace_byte_count(&small_rgba8, 1_000, PNG_DECODER_BYTE_LIMIT,),
            Some(1_076_183),
        );

        let wide_rgba16 = ApngProbeImageHeader {
            width: 16_000_000,
            height: 2,
            source_samples: 4,
            source_bit_depth: 16,
            output_samples: 4,
            interlaced: false,
        };
        assert!(
            checked_decoder_workspace_byte_count(&wide_rgba16, 1_024, PNG_DECODER_BYTE_LIMIT,)
                .is_some_and(|byte_count| byte_count > 1024 * 1024 * 1024)
        );
    }
}
