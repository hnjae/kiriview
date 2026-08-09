// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use std::{
    collections::BTreeSet,
    fs::File,
    io::{self, Cursor, Read, Seek, SeekFrom},
    path::Path,
};

use nom_exif::{
    Altitude, EntryValue, ExifIter, ExifTag, GPSInfo, IfdIndex, LatLng, LatRef, LonRef,
    MediaParser, MediaSource, TrackInfo, TrackInfoTag, URational,
};

const MAX_METADATA_READ_BYTES: usize = 4 * 1024 * 1024;
const MAX_METADATA_READ_CHUNK_BYTES: usize = 64 * 1024;
const MAX_METADATA_LABEL_BYTES: usize = 256;
const MAX_METADATA_FIELD_BYTES: usize = 8 * 1024;
const MAX_METADATA_TOTAL_BYTES: usize = 64 * 1024;
const MAX_ADVANCED_METADATA_ROWS: usize = 256;
const MAX_METADATA_ENTRY_VISITS: usize = 512;
const MAX_PRINTABLE_METADATA_ARRAY_ITEMS: usize = 64;

#[cxx::bridge(namespace = "kiriview")]
mod ffi {
    #[derive(Clone, Debug, PartialEq, Eq)]
    struct RustEmbeddedMetadataRow {
        label: String,
        value: String,
    }

    #[derive(Clone, Debug, Default, PartialEq, Eq)]
    struct RustEmbeddedMetadata {
        camera_make: String,
        camera_model: String,
        taken: String,
        location: String,
        lens: String,
        exposure: String,
        iso: String,
        focal_length: String,
        software: String,
        duration: String,
        frame_size: String,
        advanced_rows: Vec<RustEmbeddedMetadataRow>,
    }

    extern "Rust" {
        #[cxx_name = "rustParseImageEmbeddedMetadata"]
        fn rust_parse_image_embedded_metadata(data: &[u8]) -> RustEmbeddedMetadata;

        #[cxx_name = "rustParsePathEmbeddedMetadata"]
        fn rust_parse_path_embedded_metadata(path: &str) -> RustEmbeddedMetadata;
    }
}

pub(super) use ffi::{RustEmbeddedMetadata, RustEmbeddedMetadataRow};

fn rust_parse_image_embedded_metadata(data: &[u8]) -> RustEmbeddedMetadata {
    parse_image_metadata(data)
}

fn rust_parse_path_embedded_metadata(path: &str) -> RustEmbeddedMetadata {
    parse_path_metadata(path)
}

struct BoundedMetadataReader<R> {
    reader: R,
    remaining_read_bytes: usize,
}

impl<R> BoundedMetadataReader<R> {
    fn new(reader: R) -> Self {
        Self {
            reader,
            remaining_read_bytes: MAX_METADATA_READ_BYTES,
        }
    }
}

impl<R: Read> Read for BoundedMetadataReader<R> {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        if self.remaining_read_bytes == 0 || buffer.is_empty() {
            return Ok(0);
        }
        let maximum_read_bytes = buffer
            .len()
            .min(self.remaining_read_bytes)
            .min(MAX_METADATA_READ_CHUNK_BYTES);
        let read_byte_count = self.reader.read(&mut buffer[..maximum_read_bytes])?;
        self.remaining_read_bytes -= read_byte_count;
        Ok(read_byte_count)
    }
}

impl<R: Seek> Seek for BoundedMetadataReader<R> {
    fn seek(&mut self, position: SeekFrom) -> io::Result<u64> {
        self.reader.seek(position)
    }
}

fn parse_image_metadata(data: &[u8]) -> RustEmbeddedMetadata {
    let Ok(source) = MediaSource::seekable(BoundedMetadataReader::new(Cursor::new(data))) else {
        return RustEmbeddedMetadata::default();
    };
    let mut parser = MediaParser::new();
    let Ok(iter) = parser.parse_exif(source) else {
        return RustEmbeddedMetadata::default();
    };

    metadata_from_exif(iter).unwrap_or_default()
}

fn parse_path_metadata(path: &str) -> RustEmbeddedMetadata {
    let Ok(file) = File::open(Path::new(path)) else {
        return RustEmbeddedMetadata::default();
    };
    let Ok(source) = MediaSource::seekable(BoundedMetadataReader::new(file)) else {
        return RustEmbeddedMetadata::default();
    };
    let mut parser = MediaParser::new();
    let Ok(track) = parser.parse_track(source) else {
        return RustEmbeddedMetadata::default();
    };

    metadata_from_track(&track).unwrap_or_default()
}

fn metadata_from_exif(iter: ExifIter) -> Result<RustEmbeddedMetadata, ()> {
    let mut metadata = RustEmbeddedMetadata::default();
    let mut create_date = String::new();
    let mut exposure_time = None;
    let mut aperture = None;
    let mut latitude_ref = None;
    let mut latitude = None;
    let mut longitude_ref = None;
    let mut longitude = None;
    let consumed = exif_consumed_tags();

    for (entry_index, entry) in iter.enumerate() {
        if entry_index >= MAX_METADATA_ENTRY_VISITS {
            return Err(());
        }
        if entry.ifd() == IfdIndex::THUMBNAIL {
            continue;
        }
        let tag = entry.tag();
        let Ok(value) = entry.into_result() else {
            continue;
        };
        match tag.tag() {
            Some(ExifTag::Make) if metadata.camera_make.is_empty() => {
                metadata.camera_make = text(Some(&value))?;
            }
            Some(ExifTag::Model) if metadata.camera_model.is_empty() => {
                metadata.camera_model = text(Some(&value))?;
            }
            Some(ExifTag::DateTimeOriginal) if metadata.taken.is_empty() => {
                metadata.taken = datetime_text(Some(&value)).unwrap_or_default();
            }
            Some(ExifTag::CreateDate) if create_date.is_empty() => {
                create_date = datetime_text(Some(&value)).unwrap_or_default();
            }
            Some(ExifTag::LensModel) if metadata.lens.is_empty() => {
                metadata.lens = text(Some(&value))?;
            }
            Some(ExifTag::ExposureTime) if exposure_time.is_none() => {
                exposure_time = value.as_urational().and_then(exposure_time_text);
            }
            Some(ExifTag::FNumber) if aperture.is_none() => {
                aperture = value.as_urational().and_then(aperture_text);
            }
            Some(ExifTag::ISOSpeedRatings) if metadata.iso.is_empty() => {
                metadata.iso = number_text(Some(&value))?;
            }
            Some(ExifTag::FocalLength) if metadata.focal_length.is_empty() => {
                metadata.focal_length = focal_length_text(Some(&value));
            }
            Some(ExifTag::Software) if metadata.software.is_empty() => {
                metadata.software = text(Some(&value))?;
            }
            Some(ExifTag::GPSLatitudeRef) if latitude_ref.is_none() => {
                latitude_ref = value
                    .as_str()
                    .and_then(|text| text.chars().next())
                    .and_then(LatRef::from_char);
            }
            Some(ExifTag::GPSLatitude) if latitude.is_none() => {
                latitude = gps_coordinate(&value);
            }
            Some(ExifTag::GPSLongitudeRef) if longitude_ref.is_none() => {
                longitude_ref = value
                    .as_str()
                    .and_then(|text| text.chars().next())
                    .and_then(LonRef::from_char);
            }
            Some(ExifTag::GPSLongitude) if longitude.is_none() => {
                longitude = gps_coordinate(&value);
            }
            _ => {
                if consumed.contains(&tag.code()) {
                    continue;
                }
                let Some(value) = printable_value(&value)? else {
                    continue;
                };
                if metadata.advanced_rows.len() >= MAX_ADVANCED_METADATA_ROWS {
                    return Err(());
                }
                let label = tag.to_string();
                if label.len() > MAX_METADATA_LABEL_BYTES {
                    return Err(());
                }
                metadata
                    .advanced_rows
                    .push(RustEmbeddedMetadataRow { label, value });
            }
        }
    }

    if metadata.taken.is_empty() {
        metadata.taken = create_date;
    }
    if let (Some(latitude), Some(longitude)) = (latitude, longitude) {
        let gps = GPSInfo {
            latitude_ref: latitude_ref.unwrap_or(LatRef::North),
            latitude,
            longitude_ref: longitude_ref.unwrap_or(LonRef::East),
            longitude,
            altitude: Altitude::Unknown,
            speed: None,
        };
        metadata.location = location_text(Some(&gps));
    }
    metadata.exposure = match (exposure_time, aperture) {
        (Some(exposure), Some(aperture)) => format!("{exposure} at {aperture}"),
        (Some(exposure), None) => exposure,
        (None, Some(aperture)) => aperture,
        (None, None) => String::new(),
    };
    metadata_output_within_limits(&metadata)
        .then_some(metadata)
        .ok_or(())
}

fn metadata_from_track(track: &TrackInfo) -> Result<RustEmbeddedMetadata, ()> {
    let width = track.get(TrackInfoTag::Width).and_then(EntryValue::as_u32);
    let height = track.get(TrackInfoTag::Height).and_then(EntryValue::as_u32);
    let mut consumed = BTreeSet::from([
        TrackInfoTag::Make,
        TrackInfoTag::Model,
        TrackInfoTag::CreateDate,
        TrackInfoTag::Software,
    ]);
    let duration = track
        .get(TrackInfoTag::DurationMs)
        .and_then(EntryValue::as_u64)
        .map(|duration| {
            consumed.insert(TrackInfoTag::DurationMs);
            duration_text(duration)
        })
        .unwrap_or_default();
    let frame_size = width
        .zip(height)
        .map(|(width, height)| {
            consumed.insert(TrackInfoTag::Width);
            consumed.insert(TrackInfoTag::Height);
            format!("{width}×{height} px")
        })
        .unwrap_or_default();

    let metadata = RustEmbeddedMetadata {
        camera_make: track_text(track, TrackInfoTag::Make)?,
        camera_model: track_text(track, TrackInfoTag::Model)?,
        taken: track
            .get(TrackInfoTag::CreateDate)
            .and_then(|value| datetime_text(Some(value)))
            .unwrap_or_default(),
        location: location_text(track.gps_info()),
        lens: String::new(),
        exposure: String::new(),
        iso: String::new(),
        focal_length: String::new(),
        software: track_text(track, TrackInfoTag::Software)?,
        duration,
        frame_size,
        advanced_rows: track_advanced_rows(track, &consumed)?,
    };
    metadata_output_within_limits(&metadata)
        .then_some(metadata)
        .ok_or(())
}

fn text(value: Option<&EntryValue>) -> Result<String, ()> {
    match value.and_then(EntryValue::as_str) {
        Some(value) => Ok(bounded_visible_text(value)?.unwrap_or_default().to_owned()),
        None => Ok(String::new()),
    }
}

fn track_text(track: &TrackInfo, tag: TrackInfoTag) -> Result<String, ()> {
    text(track.get(tag))
}

fn number_text(value: Option<&EntryValue>) -> Result<String, ()> {
    let Some(value) = value else {
        return Ok(String::new());
    };
    let scalar = match value {
        EntryValue::U8(value) => value.to_string(),
        EntryValue::U16(value) => value.to_string(),
        EntryValue::U32(value) => value.to_string(),
        EntryValue::U64(value) => value.to_string(),
        EntryValue::I8(value) => value.to_string(),
        EntryValue::I16(value) => value.to_string(),
        EntryValue::I32(value) => value.to_string(),
        EntryValue::I64(value) => value.to_string(),
        _ => return Ok(String::new()),
    };
    Ok(bounded_owned_text(scalar)?.unwrap_or_default())
}

fn datetime_text(value: Option<&EntryValue>) -> Option<String> {
    value.and_then(EntryValue::as_datetime).map(|datetime| {
        datetime
            .into_naive()
            .format("%Y-%m-%d %H:%M:%S")
            .to_string()
    })
}

fn ratio_value(value: URational) -> Option<f64> {
    value.to_f64().filter(|value| value.is_finite())
}

fn exposure_time_text(value: URational) -> Option<String> {
    if value.denominator() == 0 {
        return None;
    }
    if value.numerator() == 1 {
        return Some(format!("1/{} s", value.denominator()));
    }
    ratio_value(value).map(|seconds| format!("{seconds:.4} s"))
}

fn aperture_text(value: URational) -> Option<String> {
    ratio_value(value).map(|aperture| format!("f/{aperture:.1}"))
}

fn focal_length_text(value: Option<&EntryValue>) -> String {
    value
        .and_then(EntryValue::as_urational)
        .and_then(ratio_value)
        .map(|length| {
            if (length.round() - length).abs() < 0.05 {
                format!("{} mm", length.round() as i64)
            } else {
                format!("{length:.1} mm")
            }
        })
        .unwrap_or_default()
}

fn gps_coordinate(value: &EntryValue) -> Option<LatLng> {
    if let Some([degrees, minutes, seconds]) = value.as_urational_slice() {
        return Some(LatLng::new(*degrees, *minutes, *seconds));
    }
    let [degrees, minutes, seconds] = value.as_irational_slice()? else {
        return None;
    };
    Some(LatLng::new(
        URational::try_from(*degrees).ok()?,
        URational::try_from(*minutes).ok()?,
        URational::try_from(*seconds).ok()?,
    ))
}

fn location_text(gps: Option<&GPSInfo>) -> String {
    let Some(gps) = gps else {
        return String::new();
    };
    let Some(latitude) = gps.latitude_decimal() else {
        return String::new();
    };
    let Some(longitude) = gps.longitude_decimal() else {
        return String::new();
    };
    format!("{latitude:.4}, {longitude:.4}")
}

fn duration_text(duration_ms: u64) -> String {
    let hours = duration_ms / 3_600_000;
    let minutes = (duration_ms / 60_000) % 60;
    let seconds = (duration_ms / 1_000) % 60;
    let milliseconds = duration_ms % 1_000;
    format!("{hours:02}:{minutes:02}:{seconds:02}.{milliseconds:03}")
}

fn bounded_visible_text(value: &str) -> Result<Option<&str>, ()> {
    let trimmed = value.trim();
    if trimmed.len() > MAX_METADATA_FIELD_BYTES {
        return Err(());
    }
    Ok((!trimmed.is_empty()
        && !trimmed.contains('\0')
        && trimmed.chars().all(|character| !character.is_control()))
    .then_some(trimmed))
}

fn bounded_owned_text(value: String) -> Result<Option<String>, ()> {
    Ok(bounded_visible_text(&value)?.map(str::to_owned))
}

fn printable_value(value: &EntryValue) -> Result<Option<String>, ()> {
    match value {
        EntryValue::Text(text) => Ok(bounded_visible_text(text)?.map(str::to_owned)),
        EntryValue::Undefined(_) | EntryValue::U8Array(_) => Ok(None),
        EntryValue::URationalArray(values)
            if values.len() <= MAX_PRINTABLE_METADATA_ARRAY_ITEMS =>
        {
            bounded_owned_text(value.to_string())
        }
        EntryValue::IRationalArray(values)
            if values.len() <= MAX_PRINTABLE_METADATA_ARRAY_ITEMS =>
        {
            bounded_owned_text(value.to_string())
        }
        EntryValue::U16Array(values) if values.len() <= MAX_PRINTABLE_METADATA_ARRAY_ITEMS => {
            bounded_owned_text(value.to_string())
        }
        EntryValue::U32Array(values) if values.len() <= MAX_PRINTABLE_METADATA_ARRAY_ITEMS => {
            bounded_owned_text(value.to_string())
        }
        EntryValue::URationalArray(_)
        | EntryValue::IRationalArray(_)
        | EntryValue::U16Array(_)
        | EntryValue::U32Array(_) => Ok(None),
        EntryValue::URational(_)
        | EntryValue::IRational(_)
        | EntryValue::U8(_)
        | EntryValue::U16(_)
        | EntryValue::U32(_)
        | EntryValue::U64(_)
        | EntryValue::I8(_)
        | EntryValue::I16(_)
        | EntryValue::I32(_)
        | EntryValue::I64(_)
        | EntryValue::F32(_)
        | EntryValue::F64(_)
        | EntryValue::DateTime(_)
        | EntryValue::NaiveDateTime(_) => bounded_owned_text(value.to_string()),
        _ => Ok(None),
    }
}

fn add_metadata_output_bytes(
    byte_count: &mut usize,
    value: &str,
    maximum_value_bytes: usize,
) -> Option<()> {
    if value.len() > maximum_value_bytes {
        return None;
    }
    *byte_count = byte_count.checked_add(value.len())?;
    (*byte_count <= MAX_METADATA_TOTAL_BYTES).then_some(())
}

fn metadata_output_within_limits(metadata: &RustEmbeddedMetadata) -> bool {
    if metadata.advanced_rows.len() > MAX_ADVANCED_METADATA_ROWS {
        return false;
    }
    let mut byte_count = 0;
    for field in [
        &metadata.camera_make,
        &metadata.camera_model,
        &metadata.taken,
        &metadata.location,
        &metadata.lens,
        &metadata.exposure,
        &metadata.iso,
        &metadata.focal_length,
        &metadata.software,
        &metadata.duration,
        &metadata.frame_size,
    ] {
        if add_metadata_output_bytes(&mut byte_count, field, MAX_METADATA_FIELD_BYTES).is_none() {
            return false;
        }
    }
    metadata.advanced_rows.iter().all(|row| {
        add_metadata_output_bytes(&mut byte_count, &row.label, MAX_METADATA_LABEL_BYTES).is_some()
            && add_metadata_output_bytes(&mut byte_count, &row.value, MAX_METADATA_FIELD_BYTES)
                .is_some()
    })
}

fn exif_consumed_tags() -> BTreeSet<u16> {
    [
        ExifTag::Make,
        ExifTag::Model,
        ExifTag::DateTimeOriginal,
        ExifTag::CreateDate,
        ExifTag::ExifOffset,
        ExifTag::GPSInfo,
        ExifTag::GPSLatitudeRef,
        ExifTag::GPSLatitude,
        ExifTag::GPSLongitudeRef,
        ExifTag::GPSLongitude,
        ExifTag::LensModel,
        ExifTag::ExposureTime,
        ExifTag::FNumber,
        ExifTag::ISOSpeedRatings,
        ExifTag::FocalLength,
        ExifTag::Software,
    ]
    .into_iter()
    .map(ExifTag::code)
    .collect()
}

fn track_advanced_rows(
    track: &TrackInfo,
    consumed: &BTreeSet<TrackInfoTag>,
) -> Result<Vec<RustEmbeddedMetadataRow>, ()> {
    let mut rows = Vec::new();
    for (tag, value) in track.iter().filter(|(tag, _)| !consumed.contains(tag)) {
        let Some(value) = printable_value(value)? else {
            continue;
        };
        if rows.len() >= MAX_ADVANCED_METADATA_ROWS {
            return Err(());
        }
        let label = tag.to_string();
        if label.len() > MAX_METADATA_LABEL_BYTES {
            return Err(());
        }
        rows.push(RustEmbeddedMetadataRow { label, value });
    }
    Ok(rows)
}

#[cfg(test)]
impl RustEmbeddedMetadata {
    fn is_empty(&self) -> bool {
        self.camera_make.is_empty()
            && self.camera_model.is_empty()
            && self.taken.is_empty()
            && self.location.is_empty()
            && self.lens.is_empty()
            && self.exposure.is_empty()
            && self.iso.is_empty()
            && self.focal_length.is_empty()
            && self.software.is_empty()
            && self.duration.is_empty()
            && self.frame_size.is_empty()
            && self.advanced_rows.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    const TIFF_HEADER_SIZE: u32 = 8;
    const IFD_ENTRY_SIZE: u32 = 12;

    #[derive(Clone)]
    struct IfdEntry {
        tag: u16,
        format: u16,
        count: u32,
        value: Vec<u8>,
    }

    struct TestExifJpegBuilder {
        ifd0: Vec<IfdEntry>,
        exif: Vec<IfdEntry>,
        gps: Vec<IfdEntry>,
    }

    impl TestExifJpegBuilder {
        fn new() -> Self {
            Self {
                ifd0: Vec::new(),
                exif: Vec::new(),
                gps: Vec::new(),
            }
        }

        fn ascii(mut self, tag: u16, value: &str) -> Self {
            let entry = ascii_entry(tag, value);
            if matches!(tag, 0x9003 | 0xa434 | 0x829a | 0x829d | 0x8827 | 0x920a) {
                self.exif.push(entry);
            } else {
                self.ifd0.push(entry);
            }
            self
        }

        fn rational(mut self, tag: u16, numerator: u32, denominator: u32) -> Self {
            self.exif.push(rational_entry(tag, numerator, denominator));
            self
        }

        fn short(mut self, tag: u16, value: u16) -> Self {
            self.exif.push(short_entry(tag, value));
            self
        }

        fn gps_ref(mut self, tag: u16, value: &str) -> Self {
            self.gps.push(ascii_entry(tag, value));
            self
        }

        fn gps_rational_triplet(mut self, tag: u16, values: [(u32, u32); 3]) -> Self {
            self.gps.push(rational_array_entry(tag, &values));
            self
        }

        fn finish(mut self) -> Vec<u8> {
            self.ifd0.push(long_entry(ExifTag::ExifOffset.code(), 0));
            self.ifd0.push(long_entry(ExifTag::GPSInfo.code(), 0));

            let ifd0_offset = TIFF_HEADER_SIZE;
            let ifd0_size = ifd_size(&self.ifd0);
            let exif_offset = ifd0_offset + ifd0_size;
            let exif_size = ifd_size(&self.exif);
            let gps_offset = exif_offset + exif_size;

            set_long_value(&mut self.ifd0, ExifTag::ExifOffset.code(), exif_offset);
            set_long_value(&mut self.ifd0, ExifTag::GPSInfo.code(), gps_offset);

            let mut tiff = Vec::new();
            tiff.extend_from_slice(b"II");
            push_u16(&mut tiff, 42);
            push_u32(&mut tiff, ifd0_offset);
            append_ifd(&mut tiff, &self.ifd0);
            append_ifd(&mut tiff, &self.exif);
            append_ifd(&mut tiff, &self.gps);

            let mut app1 = b"Exif\0\0".to_vec();
            app1.extend(tiff);

            let mut jpeg = vec![0xff, 0xd8, 0xff, 0xe1];
            push_be_u16(&mut jpeg, (app1.len() + 2) as u16);
            jpeg.extend(app1);
            jpeg.extend_from_slice(&[0xff, 0xd9]);
            jpeg
        }
    }

    fn test_exif_jpeg_builder() -> TestExifJpegBuilder {
        TestExifJpegBuilder::new()
    }

    fn ifd_size(entries: &[IfdEntry]) -> u32 {
        2 + entries.len() as u32 * IFD_ENTRY_SIZE + 4 + overflow_size(entries)
    }

    fn overflow_size(entries: &[IfdEntry]) -> u32 {
        entries
            .iter()
            .filter(|entry| entry.value.len() > 4)
            .map(|entry| entry.value.len() as u32)
            .sum()
    }

    fn append_ifd(data: &mut Vec<u8>, entries: &[IfdEntry]) {
        let base_offset = data.len() as u32;
        let value_base = base_offset + 2 + entries.len() as u32 * IFD_ENTRY_SIZE + 4;
        let mut overflow: Vec<u8> = Vec::new();
        push_u16(data, entries.len() as u16);
        for entry in entries {
            push_u16(data, entry.tag);
            push_u16(data, entry.format);
            push_u32(data, entry.count);
            if entry.value.len() <= 4 {
                data.extend(&entry.value);
                data.resize(data.len() + (4 - entry.value.len()), 0);
            } else {
                push_u32(data, value_base + overflow.len() as u32);
                overflow.extend(&entry.value);
            }
        }
        push_u32(data, 0);
        data.extend(overflow);
    }

    fn ascii_entry(tag: u16, value: &str) -> IfdEntry {
        let mut bytes = value.as_bytes().to_vec();
        bytes.push(0);
        IfdEntry {
            tag,
            format: 2,
            count: bytes.len() as u32,
            value: bytes,
        }
    }

    fn rational_entry(tag: u16, numerator: u32, denominator: u32) -> IfdEntry {
        rational_array_entry(tag, &[(numerator, denominator)])
    }

    fn rational_array_entry(tag: u16, values: &[(u32, u32)]) -> IfdEntry {
        let mut value = Vec::new();
        for (numerator, denominator) in values {
            push_u32(&mut value, *numerator);
            push_u32(&mut value, *denominator);
        }
        IfdEntry {
            tag,
            format: 5,
            count: values.len() as u32,
            value,
        }
    }

    fn short_entry(tag: u16, value: u16) -> IfdEntry {
        let mut bytes = Vec::new();
        push_u16(&mut bytes, value);
        IfdEntry {
            tag,
            format: 3,
            count: 1,
            value: bytes,
        }
    }

    fn long_entry(tag: u16, value: u32) -> IfdEntry {
        let mut bytes = Vec::new();
        push_u32(&mut bytes, value);
        IfdEntry {
            tag,
            format: 4,
            count: 1,
            value: bytes,
        }
    }

    fn set_long_value(entries: &mut [IfdEntry], tag: u16, value: u32) {
        if let Some(entry) = entries.iter_mut().find(|entry| entry.tag == tag) {
            entry.value.clear();
            push_u32(&mut entry.value, value);
        }
    }

    fn push_u16(data: &mut Vec<u8>, value: u16) {
        data.extend_from_slice(&value.to_le_bytes());
    }

    fn push_u32(data: &mut Vec<u8>, value: u32) {
        data.extend_from_slice(&value.to_le_bytes());
    }

    fn push_be_u16(data: &mut Vec<u8>, value: u16) {
        data.extend_from_slice(&value.to_be_bytes());
    }

    fn push_be_u32(data: &mut Vec<u8>, value: u32) {
        data.extend_from_slice(&value.to_be_bytes());
    }

    fn append_box(data: &mut Vec<u8>, kind: &[u8; 4], payload: &[u8]) {
        push_be_u32(data, (payload.len() + 8) as u32);
        data.extend_from_slice(kind);
        data.extend_from_slice(payload);
    }

    fn jpeg_with_exif_metadata() -> Vec<u8> {
        test_exif_jpeg_builder()
            .ascii(0x010f, "Kiri Camera Co.")
            .ascii(0x0110, "KiriCam 1")
            .ascii(0x010e, "Advanced note")
            .ascii(0x013c, "")
            .ascii(0x0131, "KiriOS Camera")
            .ascii(0x9003, "2026:05:31 12:34:56")
            .ascii(0xa434, "Kiri Prime 35mm")
            .rational(0x829a, 1, 125)
            .rational(0x829d, 56, 10)
            .short(0x8827, 400)
            .rational(0x920a, 35, 1)
            .gps_ref(0x0001, "N")
            .gps_rational_triplet(0x0002, [(37, 1), (46, 1), (2964, 100)])
            .gps_ref(0x0003, "W")
            .gps_rational_triplet(0x0004, [(122, 1), (25, 1), (984, 100)])
            .finish()
    }

    fn tiny_metadata_mp4() -> Vec<u8> {
        let mut ftyp_payload = Vec::new();
        ftyp_payload.extend_from_slice(b"isom");
        ftyp_payload.extend_from_slice(&[0; 4]);
        ftyp_payload.extend_from_slice(b"isomiso2mp41");

        let mut mvhd_payload = vec![0; 12];
        push_be_u32(&mut mvhd_payload, 1000);
        push_be_u32(&mut mvhd_payload, 1234);
        mvhd_payload.extend_from_slice(&[0; 80]);

        let mut tkhd_payload = vec![0, 0, 0, 7];
        tkhd_payload.extend_from_slice(&[0; 16]);
        push_be_u32(&mut tkhd_payload, 1234);
        tkhd_payload.extend_from_slice(&[0; 52]);
        push_be_u32(&mut tkhd_payload, 640 << 16);
        push_be_u32(&mut tkhd_payload, 360 << 16);

        let mut hdlr_payload = vec![0; 8];
        hdlr_payload.extend_from_slice(b"vide");
        hdlr_payload.extend_from_slice(&[0; 12]);
        let mut mdia_payload = Vec::new();
        append_box(&mut mdia_payload, b"hdlr", &hdlr_payload);

        let mut trak_payload = Vec::new();
        append_box(&mut trak_payload, b"tkhd", &tkhd_payload);
        append_box(&mut trak_payload, b"mdia", &mdia_payload);

        let mut moov_payload = Vec::new();
        append_box(&mut moov_payload, b"mvhd", &mvhd_payload);
        append_box(&mut moov_payload, b"trak", &trak_payload);

        let mut data = Vec::new();
        append_box(&mut data, b"ftyp", &ftyp_payload);
        append_box(&mut data, b"moov", &moov_payload);
        data
    }

    #[test]
    fn invalid_image_bytes_return_empty_metadata() {
        let metadata = parse_image_metadata(b"not an image");

        assert!(metadata.is_empty());
    }

    #[test]
    fn metadata_reader_enforces_one_cumulative_read_budget() {
        let data = vec![0_u8; MAX_METADATA_READ_BYTES + 1024];
        let mut reader = BoundedMetadataReader::new(Cursor::new(data.as_slice()));
        let mut first_pass = Vec::new();

        reader
            .read_to_end(&mut first_pass)
            .expect("bounded metadata bytes should be readable");
        assert_eq!(first_pass.len(), MAX_METADATA_READ_BYTES);

        reader
            .seek(SeekFrom::Start(0))
            .expect("bounded metadata source should remain seekable");
        let mut after_seek = [0_u8; 1];
        assert_eq!(reader.read(&mut after_seek).unwrap(), 0);
    }

    #[test]
    fn metadata_projection_rejects_oversized_fields_and_bounds_total_output() {
        let oversized_make = "x".repeat(MAX_METADATA_FIELD_BYTES + 1);
        let metadata = parse_image_metadata(
            &test_exif_jpeg_builder()
                .ascii(0x010f, &oversized_make)
                .finish(),
        );
        assert!(metadata.is_empty());

        let projected = RustEmbeddedMetadata {
            advanced_rows: (0..MAX_ADVANCED_METADATA_ROWS)
                .map(|index| RustEmbeddedMetadataRow {
                    label: format!("Row {index}"),
                    value: "v".repeat(MAX_METADATA_FIELD_BYTES),
                })
                .collect(),
            ..RustEmbeddedMetadata::default()
        };
        assert!(!metadata_output_within_limits(&projected));
    }

    #[test]
    fn too_many_advanced_rows_reject_the_complete_projection() {
        let mut builder = test_exif_jpeg_builder();
        for index in 0..=MAX_ADVANCED_METADATA_ROWS {
            builder = builder.ascii(0xd000 + index as u16, "value");
        }

        assert!(parse_image_metadata(&builder.finish()).is_empty());
    }

    #[test]
    fn too_many_parsed_entries_reject_curated_output() {
        let mut builder = test_exif_jpeg_builder().ascii(0x010f, "Kiri Camera");
        for index in 0..MAX_METADATA_ENTRY_VISITS {
            builder = builder.ascii(0xe000 + index as u16, "");
        }

        assert!(parse_image_metadata(&builder.finish()).is_empty());
    }

    #[test]
    fn advanced_array_projection_is_bounded_before_formatting() {
        let small = EntryValue::U16Array(vec![1; MAX_PRINTABLE_METADATA_ARRAY_ITEMS]);
        let large = EntryValue::U16Array(vec![1; MAX_PRINTABLE_METADATA_ARRAY_ITEMS + 1]);

        assert!(printable_value(&small).unwrap().is_some());
        assert!(printable_value(&large).unwrap().is_none());
    }

    #[test]
    fn jpeg_exif_camera_fields_are_curated() {
        let metadata = parse_image_metadata(&jpeg_with_exif_metadata());

        assert_eq!(metadata.camera_make, "Kiri Camera Co.");
        assert_eq!(metadata.camera_model, "KiriCam 1");
        assert_eq!(metadata.lens, "Kiri Prime 35mm");
        assert_eq!(metadata.exposure, "1/125 s at f/5.6");
        assert_eq!(metadata.iso, "400");
        assert_eq!(metadata.focal_length, "35 mm");
        assert_eq!(metadata.software, "KiriOS Camera");
    }

    #[test]
    fn jpeg_exif_gps_and_date_fields_are_curated() {
        let metadata = parse_image_metadata(&jpeg_with_exif_metadata());

        assert_eq!(metadata.taken, "2026-05-31 12:34:56");
        assert_eq!(metadata.location, "37.7749, -122.4194");
    }

    #[test]
    fn advanced_rows_skip_curated_and_empty_values() {
        let metadata = parse_image_metadata(&jpeg_with_exif_metadata());
        let curated_labels: BTreeSet<String> = [
            ExifTag::Make,
            ExifTag::Model,
            ExifTag::DateTimeOriginal,
            ExifTag::CreateDate,
            ExifTag::GPSInfo,
            ExifTag::GPSLatitudeRef,
            ExifTag::GPSLatitude,
            ExifTag::GPSLongitudeRef,
            ExifTag::GPSLongitude,
            ExifTag::LensModel,
            ExifTag::ExposureTime,
            ExifTag::FNumber,
            ExifTag::ISOSpeedRatings,
            ExifTag::FocalLength,
            ExifTag::Software,
        ]
        .into_iter()
        .map(|tag| tag.to_string())
        .collect();

        assert!(
            metadata
                .advanced_rows
                .iter()
                .all(|row| !row.value.is_empty())
        );
        assert!(
            metadata
                .advanced_rows
                .iter()
                .all(|row| !curated_labels.contains(&row.label))
        );
        assert!(
            metadata
                .advanced_rows
                .iter()
                .any(|row| row.label == ExifTag::ImageDescription.to_string()
                    && row.value == "Advanced note")
        );
        assert!(
            metadata
                .advanced_rows
                .iter()
                .all(|row| row.label != ExifTag::HostComputer.to_string())
        );
    }

    #[test]
    fn missing_video_path_returns_empty_metadata() {
        let temp = tempfile::TempDir::new().expect("temporary directory should be created");
        let missing = temp.path().join("missing-video.mp4");
        let metadata = parse_path_metadata(
            missing
                .to_str()
                .expect("temporary path should be valid UTF-8"),
        );

        assert!(metadata.is_empty());
    }

    #[test]
    fn direct_video_track_metadata_is_curated() {
        let file = tempfile::Builder::new()
            .suffix(".mp4")
            .tempfile()
            .expect("temporary video should be created");
        fs::write(file.path(), tiny_metadata_mp4()).expect("test mp4 should be written");

        let metadata = parse_path_metadata(
            file.path()
                .to_str()
                .expect("temporary path should be valid UTF-8"),
        );

        assert_eq!(metadata.duration, "00:00:01.234");
        assert_eq!(metadata.frame_size, "640×360 px");
    }
}
