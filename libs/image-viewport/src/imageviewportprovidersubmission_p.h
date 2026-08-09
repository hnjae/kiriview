/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequenceprovider.h>

#include <functional>

namespace ImageViewportInternal {

class ProviderEventSubmissionPrivateAccess
{
public:
    using Submitter = std::function<ImageSequenceProviderEventSubmissionOutcome(
        const ImageSequenceProviderEvent&)>;

    static void install(ImageSequenceProviderSession& session, Submitter submitter);
};

} // namespace ImageViewportInternal
