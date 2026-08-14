/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportprovidersubmission_p.h"

class RevisionToken
{
public:
    RevisionToken() = default;

    [[nodiscard]] bool isValid() const { return m_value != 0; }

    friend bool operator==(RevisionToken lhs, RevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

private:
    explicit RevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportTypesPrivateAccess;
};

class ImageViewportTypesPrivateAccess
{
public:
    static RevisionToken revisionTokenFromValue(quint64 value) { return RevisionToken(value); }
    static ImageViewportRevisionToken publicRevisionFromValue(quint64 value)
    {
        return ImageViewportRevisionToken(value);
    }
    static quint64 value(RevisionToken token) { return token.m_value; }
    static quint64 value(ImageViewportRevisionToken token) { return token.m_value; }
    static ImageViewportPresentationTargetGenerationToken fromValue(quint64 value)
    {
        return ImageViewportPresentationTargetGenerationToken(value);
    }
    static ImageViewportDemandRevisionToken demandRevisionFromValue(quint64 value)
    {
        return ImageViewportDemandRevisionToken(value);
    }

    static quint64 value(ImageViewportDemandRevisionToken token) { return token.m_value; }
    static ImageViewportAllocationGenerationToken allocationGenerationFromValue(quint64 value)
    {
        return ImageViewportAllocationGenerationToken(value);
    }
};

namespace ImageViewportInternal {

class RevisionTokenPrivateAccess
{
public:
    static RevisionToken fromValue(quint64 value)
    {
        return ImageViewportTypesPrivateAccess::revisionTokenFromValue(value);
    }
    static ImageViewportRevisionToken publicRevisionFromValue(quint64 value)
    {
        return ImageViewportTypesPrivateAccess::publicRevisionFromValue(value);
    }
    static quint64 value(RevisionToken token)
    {
        return ImageViewportTypesPrivateAccess::value(token);
    }
    static quint64 value(ImageViewportRevisionToken token)
    {
        return ImageViewportTypesPrivateAccess::value(token);
    }
};

class PresentationTargetGenerationTokenPrivateAccess
{
public:
    static ImageViewportPresentationTargetGenerationToken fromValue(quint64 value)
    {
        return ImageViewportTypesPrivateAccess::fromValue(value);
    }
};

class DemandRevisionTokenPrivateAccess
{
public:
    static ImageViewportDemandRevisionToken fromValue(quint64 value)
    {
        return ImageViewportTypesPrivateAccess::demandRevisionFromValue(value);
    }

    static quint64 value(ImageViewportDemandRevisionToken token)
    {
        return ImageViewportTypesPrivateAccess::value(token);
    }
};

class AllocationGenerationTokenPrivateAccess
{
public:
    static ImageViewportAllocationGenerationToken fromValue(quint64 value)
    {
        return ImageViewportTypesPrivateAccess::allocationGenerationFromValue(value);
    }
};

} // namespace ImageViewportInternal
