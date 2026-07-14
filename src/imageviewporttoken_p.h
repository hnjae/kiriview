#pragma once

#include <ImageViewport/ImageViewport>

class RevisionToken
{
public:
    RevisionToken() = default;

    bool isValid() const { return m_value != 0; }

    friend bool operator==(RevisionToken lhs, RevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(RevisionToken lhs, RevisionToken rhs) { return !(lhs == rhs); }

private:
    explicit RevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportInternal::RevisionTokenPrivateAccess;
};

namespace ImageViewportInternal {

class ProviderRequestTokenPrivateAccess
{
public:
    static ImageSequenceProviderRequestToken fromValue(quint64 value)
    {
        return ImageSequenceProviderRequestToken(value);
    }

    static quint64 value(ImageSequenceProviderRequestToken token) { return token.m_id; }
};

class RevisionTokenPrivateAccess
{
public:
    static RevisionToken fromValue(quint64 value) { return RevisionToken(value); }
    static ImageViewportRevisionToken publicRevisionFromValue(quint64 value)
    {
        return ImageViewportRevisionToken(value);
    }
    static ImageViewportPresentationTargetGenerationToken generationFromValue(quint64 value)
    {
        return ImageViewportPresentationTargetGenerationToken(value);
    }
    static ImageViewportDemandRevisionToken demandFromValue(quint64 value)
    {
        return ImageViewportDemandRevisionToken(value);
    }

    static quint64 value(RevisionToken token) { return token.m_value; }
    static quint64 value(ImageViewportRevisionToken token) { return token.m_value; }
};

} // namespace ImageViewportInternal
