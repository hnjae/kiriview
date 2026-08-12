/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <QtCore/QObject>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

namespace ImageViewportInternal {
class ImageSequencePrivateAccess;
}

namespace ImageSequenceEnums {
Q_NAMESPACE

enum class AuthoredAnimationLoopMode {
    Unavailable,
    PlayOnce,
    Finite,
    Infinite,
};
Q_ENUM_NS(AuthoredAnimationLoopMode)
}

using ImageSequenceAuthoredAnimationLoopMode = ImageSequenceEnums::AuthoredAnimationLoopMode;

class ImageSequenceAuthoredAnimationFacts
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceAuthoredAnimationFacts)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    ImageSequenceAuthoredAnimationFacts() = default;
    static ImageSequenceAuthoredAnimationFacts finiteLoop(int loopCount);
    static ImageSequenceAuthoredAnimationFacts infiniteLoop();

    [[nodiscard]] bool autoplay() const;
    void setAutoplay(bool autoplay);
    [[nodiscard]] ImageSequenceAuthoredAnimationLoopMode loopMode() const;
    [[nodiscard]] int loopCount() const;
    bool setFiniteLoopCount(int loopCount);
    [[nodiscard]] bool isValid() const;

private:
    bool m_autoplay = false;
    ImageSequenceAuthoredAnimationLoopMode m_loopMode
        = ImageSequenceAuthoredAnimationLoopMode::PlayOnce;
    int m_loopCount = 1;
};

class ImageSequence : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use ImageSequenceFactory to create sequence handles")

public:
    ~ImageSequence() override;
    Q_DISABLE_COPY_MOVE(ImageSequence)

private:
    class Data;
    explicit ImageSequence(std::unique_ptr<Data> data, QObject* parent = nullptr);
    static void deleteData(Data* data);

    std::unique_ptr<Data, void (*)(Data*)> d;

    friend class ImageViewportInternal::ImageSequencePrivateAccess;
};

Q_DECLARE_METATYPE(ImageSequenceAuthoredAnimationFacts)
