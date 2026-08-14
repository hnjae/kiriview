// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewportstate.h>

ImageViewportRoleSnapshot::ImageViewportRoleSnapshot(bool present, ImageSequence* sequence,
    ImageViewportRoleRequestSnapshot request, ImageViewportRoleDisplaySnapshot display,
    ImageViewportRoleMetadataSnapshot metadata, ImageViewportRoleGeometrySnapshot geometry)
    : m_present(present)
    , m_sequence(sequence)
    , m_request(request)
    , m_display(display)
    , m_metadata(metadata)
    , m_geometry(geometry)
{
}

ImageSequence* ImageViewportRoleSnapshot::sequence() const
{
    return static_cast<ImageSequence*>(m_sequence.data());
}
