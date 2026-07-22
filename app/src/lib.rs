// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

// Capability implementations behind the allowlisted C++ support boundary.
#[path = "support/apnganimationreader.rs"]
mod apnganimationreader;
#[path = "support/embeddedmetadata.rs"]
mod embeddedmetadata;
#[path = "support/svgrenderer.rs"]
mod svgrenderer;
#[path = "support/thumbnailcache.rs"]
mod thumbnailcache;
