#pragma once

#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <memory>

class ImageSequenceProviderAdapter;
Q_DECLARE_OPAQUE_POINTER(ImageSequenceProviderAdapter*)
namespace ImageViewportInternal {
class ImageFramePrivateAccess;
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

    bool autoplay() const;
    void setAutoplay(bool autoplay);
    ImageSequenceAuthoredAnimationLoopMode loopMode() const;
    int loopCount() const;
    bool setFiniteLoopCount(int loopCount);
    bool isValid() const;

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

private:
    class Data;
    explicit ImageSequence(std::unique_ptr<Data> data, QObject* parent = nullptr);

    std::unique_ptr<Data> d;

    friend class ImageViewportInternal::ImageSequencePrivateAccess;
};

class ImageFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageFrame objects are created by C++ helpers or provider adapters")
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(qint64 payloadByteSize READ payloadByteSize CONSTANT)
    Q_PROPERTY(QSizeF payloadRasterSize READ payloadRasterSize CONSTANT)
    Q_PROPERTY(QSizeF sourceToPayloadScale READ sourceToPayloadScale CONSTANT)
    Q_PROPERTY(ImageViewportPayloadQuality quality READ quality CONSTANT)
    Q_PROPERTY(ImageViewportPayloadExactness exactness READ exactness CONSTANT)
    Q_PROPERTY(bool hasAlpha READ hasAlpha CONSTANT)
    Q_PROPERTY(OrientationPolicy orientationPolicy READ orientationPolicy CONSTANT)
    Q_PROPERTY(QString formatIdentifier READ formatIdentifier CONSTANT)

public:
    enum class OrientationPolicy {
        Identity,
        MirrorHorizontally,
        MirrorVertically,
        Rotate180,
        Rotate90,
        MirrorHorizontallyAndRotate90,
        MirrorVerticallyAndRotate90,
        Rotate270,
    };
    Q_ENUM(OrientationPolicy)

    explicit ImageFrame(QObject* parent = nullptr);
    explicit ImageFrame(const QImage& image, QObject* parent = nullptr);
    ImageFrame(const QImage& image, OrientationPolicy orientationPolicy, QObject* parent = nullptr);
    ImageFrame(const QImage& image, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
        QSizeF sourceToPayloadScale, qint64 payloadByteSize, ImageViewportPayloadQuality quality,
        ImageViewportPayloadExactness exactness, bool hasAlpha, OrientationPolicy orientationPolicy,
        QString formatIdentifier, QObject* parent = nullptr);

    bool isValid() const;
    QSizeF sourceLogicalSize() const;
    qint64 payloadByteSize() const;
    QSizeF payloadRasterSize() const;
    QSizeF sourceToPayloadScale() const;
    ImageViewportPayloadQuality quality() const;
    ImageViewportPayloadExactness exactness() const;
    bool hasAlpha() const;
    OrientationPolicy orientationPolicy() const;
    QString formatIdentifier() const;

private:
    ImageFrame(const QImage& image, qsizetype payloadByteSizeOverride, QObject* parent = nullptr);
    const QImage& imagePayload() const;

    QImage m_image;
    QSizeF m_logicalSize;
    qint64 m_payloadByteSize = 0;
    QSizeF m_payloadRasterSize;
    QSizeF m_sourceToPayloadScale;
    ImageViewportPayloadQuality m_quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness m_exactness = ImageViewportPayloadExactness::Unknown;
    bool m_hasAlpha = false;
    OrientationPolicy m_orientationPolicy = OrientationPolicy::Identity;
    QString m_formatIdentifier;

    friend class ImageSequenceFactory;
    friend class TimedImageFrame;
    friend class TimedImageFrameList;
    friend class ImageViewport;
    friend class ImageViewportInternal::ImageFramePrivateAccess;
};

class TimedImageFrame
{
    Q_GADGET
    QML_VALUE_TYPE(timedImageFrame)
    Q_PROPERTY(ImageFrame* frame READ frame CONSTANT)
    Q_PROPERTY(int startPosition READ startPosition CONSTANT)
    Q_PROPERTY(int duration READ duration CONSTANT)

public:
    TimedImageFrame() = default;
    TimedImageFrame(ImageFrame* frame, int startPosition, int duration);

    ImageFrame* frame() const;
    int startPosition() const;
    int duration() const;
    bool isValid() const;

private:
    std::shared_ptr<ImageFrame> m_frame;
    int m_startPosition = -1;
    int m_duration = -1;
};

class TimedImageFrameList : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QList<TimedImageFrame> frames READ frames NOTIFY countChanged)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY animationFactsChanged)
    Q_PROPERTY(
        ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode NOTIFY animationFactsChanged)
    Q_PROPERTY(int loopCount READ loopCount NOTIFY animationFactsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)

public:
    explicit TimedImageFrameList(QObject* parent = nullptr);

    int count() const;
    QList<TimedImageFrame> frames() const;
    QString errorString() const;
    bool autoplay() const;
    void setAutoplay(bool autoplay);
    ImageSequenceAuthoredAnimationLoopMode loopMode() const;
    int loopCount() const;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts() const;
    void setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    bool appendFrame(const QImage& image, int durationMilliseconds);
    Q_INVOKABLE bool appendFrame(ImageFrame* frame, int durationMilliseconds);
    Q_INVOKABLE bool appendFrame(const TimedImageFrame& frame);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void animationFactsChanged();
    void diagnosticsChanged();

private:
    bool isValid() const;
    QSizeF logicalSize() const;
    QVector<int> frameDurations() const;
    QVector<QImage> frameImages() const;
    QVector<qint64> framePayloadByteSizes() const;
    int totalDuration() const;
    void setErrorString(const QString& errorString);

    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;
    QVector<QImage> m_images;
    QVector<qint64> m_payloadByteSizes;
    QList<TimedImageFrame> m_frames;
    ImageSequenceAuthoredAnimationFacts m_authoredAnimationFacts;
    QString m_errorString;

    friend class ImageSequenceFactory;
};

namespace ImageSequenceFactoryEnums {
Q_NAMESPACE

enum class FactoryOutcome {
    Created,
    Rejected,
};
Q_ENUM_NS(FactoryOutcome)

enum class FactoryReason {
    NoError,
    InvalidFrame,
    InvalidTiming,
    InvalidAnimationMetadata,
    InvalidProviderDescriptor,
    LimitExceeded,
};
Q_ENUM_NS(FactoryReason)
}

using ImageSequenceFactoryOutcome = ImageSequenceFactoryEnums::FactoryOutcome;
using ImageSequenceFactoryReason = ImageSequenceFactoryEnums::FactoryReason;

class ImageSequenceFactoryResult : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageSequenceFactoryResult objects are returned by ImageSequenceFactory")
    QML_EXTENDED_NAMESPACE(ImageSequenceFactoryEnums)
    Q_PROPERTY(ImageSequence* sequence READ sequence CONSTANT)
    Q_PROPERTY(ImageSequenceFactoryOutcome outcome READ outcome CONSTANT)
    Q_PROPERTY(ImageSequenceFactoryReason reason READ reason CONSTANT)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)

public:
    explicit ImageSequenceFactoryResult(ImageSequence* sequence,
        ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason,
        QString errorString = {}, QObject* parent = nullptr);

    ImageSequence* sequence() const;
    ImageSequenceFactoryOutcome outcome() const;
    ImageSequenceFactoryReason reason() const;
    QString errorString() const;

private:
    friend class ImageSequenceFactory;

    explicit ImageSequenceFactoryResult(std::shared_ptr<ImageSequence> sequence,
        ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason,
        QString errorString = {}, QObject* parent = nullptr);

    QPointer<ImageSequence> m_sequence;
    std::shared_ptr<ImageSequence> m_sequenceOwner;
    ImageSequenceFactoryOutcome m_outcome = ImageSequenceFactoryOutcome::Rejected;
    ImageSequenceFactoryReason m_reason = ImageSequenceFactoryReason::InvalidFrame;
    QString m_errorString;
};

class ImageSequenceFactory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ImageSequenceFactory(QObject* parent = nullptr);

    ImageSequenceFactoryResult* fromFrame(const QImage& image);
    ImageSequenceFactoryResult* fromTimedFrameList(
        const QVector<QImage>& images, const QVector<int>& durationsMilliseconds);
    Q_INVOKABLE ImageSequenceFactoryResult* fromFrame(ImageFrame* frame);
    Q_INVOKABLE ImageSequenceFactoryResult* fromTimedFrameList(TimedImageFrameList* list);
    Q_INVOKABLE ImageSequenceFactoryResult* fromProvider(ImageSequenceProviderAdapter* adapter);
};

class ImageSequenceLimits : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int maximumSourceLogicalWidth READ getMaximumSourceLogicalWidth CONSTANT)
    Q_PROPERTY(int maximumSourceLogicalHeight READ getMaximumSourceLogicalHeight CONSTANT)
    Q_PROPERTY(qint64 maximumSourceLogicalPixels READ getMaximumSourceLogicalPixels CONSTANT)
    Q_PROPERTY(int maximumPayloadRasterWidth READ getMaximumPayloadRasterWidth CONSTANT)
    Q_PROPERTY(int maximumPayloadRasterHeight READ getMaximumPayloadRasterHeight CONSTANT)
    Q_PROPERTY(qint64 maximumPayloadBytes READ getMaximumPayloadBytes CONSTANT)
    Q_PROPERTY(int maximumFrameCount READ getMaximumFrameCount CONSTANT)
    Q_PROPERTY(
        int maximumFrameDurationMilliseconds READ getMaximumFrameDurationMilliseconds CONSTANT)
    Q_PROPERTY(
        int maximumTotalDurationMilliseconds READ getMaximumTotalDurationMilliseconds CONSTANT)
    Q_PROPERTY(int maximumDiagnosticCharacters READ getMaximumDiagnosticCharacters CONSTANT)
    Q_PROPERTY(
        int maximumFormatIdentifierCharacters READ getMaximumFormatIdentifierCharacters CONSTANT)

public:
    explicit ImageSequenceLimits(QObject* parent = nullptr);

    int getMaximumSourceLogicalWidth() const;
    int getMaximumSourceLogicalHeight() const;
    qint64 getMaximumSourceLogicalPixels() const;
    int getMaximumPayloadRasterWidth() const;
    int getMaximumPayloadRasterHeight() const;
    qint64 getMaximumPayloadBytes() const;
    int getMaximumFrameCount() const;
    int getMaximumFrameDurationMilliseconds() const;
    int getMaximumTotalDurationMilliseconds() const;
    int getMaximumDiagnosticCharacters() const;
    int getMaximumFormatIdentifierCharacters() const;

    static int maximumSourceLogicalWidth();
    static int maximumSourceLogicalHeight();
    static qint64 maximumSourceLogicalPixels();
    static int maximumPayloadRasterWidth();
    static int maximumPayloadRasterHeight();
    static qint64 maximumPayloadBytes();
    static int maximumFrameCount();
    static int maximumFrameDurationMilliseconds();
    static int maximumTotalDurationMilliseconds();
    static int maximumDiagnosticCharacters();
    static int maximumFormatIdentifierCharacters();
};

Q_DECLARE_METATYPE(ImageSequenceAuthoredAnimationFacts)
