/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequenceprovider.h>

#include <functional>
#include <utility>

class ImageSequenceProviderPrivateAccess
{
public:
    using Submitter = std::function<ImageSequenceProviderEventSubmissionOutcome(
        const ImageSequenceProviderEvent&)>;

    static ImageSequenceProviderRequestToken requestTokenFromValue(quint64 value)
    {
        return ImageSequenceProviderRequestToken(value);
    }
    static quint64 value(ImageSequenceProviderRequestToken token) { return token.m_id; }
    static void install(ImageSequenceProviderSession& session, Submitter submitter);
};

namespace ImageViewportInternal {

class ProviderRequestTokenPrivateAccess
{
public:
    static ImageSequenceProviderRequestToken fromValue(quint64 value)
    {
        return ImageSequenceProviderPrivateAccess::requestTokenFromValue(value);
    }

    static quint64 value(ImageSequenceProviderRequestToken token)
    {
        return ImageSequenceProviderPrivateAccess::value(token);
    }
};

class ProviderEventSubmissionPrivateAccess
{
public:
    using Submitter = ImageSequenceProviderPrivateAccess::Submitter;

    static void install(ImageSequenceProviderSession& session, Submitter submitter)
    {
        ImageSequenceProviderPrivateAccess::install(session, std::move(submitter));
    }
};

} // namespace ImageViewportInternal
