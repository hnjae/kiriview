#include "imageviewport.h"

#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGNode>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/QSGTexture>
#include <QtQuick/QQuickWindow>
#include <QtCore/QRegularExpression>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int minimumMaximumLogicalSide = 8192;
constexpr qint64 minimumMaximumPixelsPerFrame = 67108864LL;
constexpr qint64 minimumMaximumPayloadBytesPerFrame = 268435456LL;
constexpr int minimumMaximumTimedListFrameCount = 10000;
constexpr int minimumMaximumDuration = 86400000;
constexpr int minimumMaximumDiagnosticStringLength = 4096;

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool isPositiveFiniteInteger(double value)
{
    return std::isfinite(value) && value > 0.0 && std::trunc(value) == value;
}

bool isAdmittedLogicalSizeComponent(double value, int maximum)
{
    return isPositiveFiniteInteger(value) && value <= static_cast<double>(maximum);
}

bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QString frameLimitViolation(const ImageFrame &frame)
{
    const QSizeF size = frame.logicalSize();
    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width > ImageSequenceLimits::maximumLogicalWidth()) {
        return QStringLiteral("ImageFrame exceeds maximumLogicalWidth");
    }
    if (height > ImageSequenceLimits::maximumLogicalHeight()) {
        return QStringLiteral("ImageFrame exceeds maximumLogicalHeight");
    }
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return QStringLiteral("ImageFrame exceeds maximumPixelsPerFrame");
    }
    if (frame.payloadByteSize() <= 0) {
        return QStringLiteral("ImageFrame payload byte size is invalid");
    }
    if (frame.payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        return QStringLiteral("ImageFrame exceeds maximumPayloadBytesPerFrame");
    }

    return {};
}

QString providerMetadataLimitViolation(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isSpecified()) {
        return {};
    }
    if (!metadata.isValid()) {
        return QStringLiteral("provider metadata is invalid");
    }

    const QSizeF size = metadata.logicalSize();
    if (!isPositiveFiniteInteger(size.width()) || !isPositiveFiniteInteger(size.height())) {
        return QStringLiteral("provider metadata is invalid");
    }
    if (size.width() > ImageSequenceLimits::maximumLogicalWidth()) {
        return QStringLiteral("provider metadata logical width exceeds maximumLogicalWidth");
    }
    if (size.height() > ImageSequenceLimits::maximumLogicalHeight()) {
        return QStringLiteral("provider metadata logical height exceeds maximumLogicalHeight");
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return QStringLiteral("provider metadata logical size exceeds maximumPixelsPerFrame");
    }

    if (!metadata.isTimedFrameList()) {
        return {};
    }

    const QVector<int> durations = metadata.frameDurations();
    if (durations.size() > ImageSequenceLimits::maximumTimedListFrameCount()) {
        return QStringLiteral("provider metadata frame count exceeds maximumTimedListFrameCount");
    }

    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration <= 0) {
            return QStringLiteral("provider metadata frame duration must be positive");
        }
        if (duration > ImageSequenceLimits::maximumFrameDuration()) {
            return QStringLiteral("provider metadata frame duration exceeds maximumFrameDuration");
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalSequenceDuration()) {
            return QStringLiteral("provider metadata total duration exceeds maximumTotalSequenceDuration");
        }
    }

    return {};
}

ImageViewport::TriState capabilitySupportToTriState(ImageSequenceProviderCapabilitySupport support)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::DeclaredFalse:
        return ImageViewport::TriState::False;
    case ImageSequenceProviderCapabilitySupport::DeclaredTrue:
        return ImageViewport::TriState::True;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return ImageViewport::TriState::Unavailable;
    }

    return ImageViewport::TriState::Unavailable;
}

bool providerCapabilityContradictsMetadata(ImageSequenceProviderCapabilitySupport support, bool metadataCapability)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::DeclaredFalse:
        return metadataCapability;
    case ImageSequenceProviderCapabilitySupport::DeclaredTrue:
        return !metadataCapability;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return false;
    }

    return false;
}

bool isValidFillMode(ImageViewport::FillMode mode)
{
    switch (mode) {
    case ImageViewport::FillMode::Contain:
    case ImageViewport::FillMode::Cover:
    case ImageViewport::FillMode::Stretch:
    case ImageViewport::FillMode::Center:
        return true;
    }

    return false;
}

bool isValidHorizontalAlignment(ImageViewport::HorizontalAlignment alignment)
{
    switch (alignment) {
    case ImageViewport::HorizontalAlignment::AlignLeft:
    case ImageViewport::HorizontalAlignment::AlignHCenter:
    case ImageViewport::HorizontalAlignment::AlignRight:
        return true;
    }

    return false;
}

bool isValidVerticalAlignment(ImageViewport::VerticalAlignment alignment)
{
    switch (alignment) {
    case ImageViewport::VerticalAlignment::AlignTop:
    case ImageViewport::VerticalAlignment::AlignVCenter:
    case ImageViewport::VerticalAlignment::AlignBottom:
        return true;
    }

    return false;
}

bool isValidBackgroundMode(ImageViewport::BackgroundMode mode)
{
    switch (mode) {
    case ImageViewport::BackgroundMode::Transparent:
    case ImageViewport::BackgroundMode::SolidColor:
    case ImageViewport::BackgroundMode::Checkerboard:
        return true;
    }

    return false;
}

QString redactDiagnosticDetails(QString diagnostic)
{
    static const QRegularExpression credentialPattern(
        QStringLiteral("\\b(?:password|passwd|pwd|token|api[_-]?key|secret)\\s*[:=]\\s*\\S+"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression urlPattern(QStringLiteral("\\b[A-Za-z][A-Za-z0-9+.-]*://\\S+"));
    static const QRegularExpression windowsPathPattern(QStringLiteral("\\b[A-Za-z]:[\\\\/][^\\s]+"));
    static const QRegularExpression unixPathPattern(QStringLiteral("(?<!\\w)/(?:[^\\s/]+/)+[^\\s]+"));

    diagnostic.replace(credentialPattern, QStringLiteral("[redacted-credential]"));
    diagnostic.replace(urlPattern, QStringLiteral("[redacted-url]"));
    diagnostic.replace(windowsPathPattern, QStringLiteral("[redacted-path]"));
    diagnostic.replace(unixPathPattern, QStringLiteral("[redacted-path]"));
    return diagnostic;
}

}

ImageSequence::ImageSequence(QObject *parent)
    : QObject(parent)
{
}

ImageSequence::ImageSequence(const QSizeF &logicalSize, QImage stillImage, QObject *parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Still)
    , m_logicalSize(logicalSize)
    , m_stillImage(std::move(stillImage))
{
}

ImageSequence::ImageSequence(const QSizeF &logicalSize, QVector<int> frameDurations, QVector<QImage> frameImages, QObject *parent)
    : QObject(parent)
    , m_timingModel(TimingModel::TimedList)
    , m_logicalSize(logicalSize)
    , m_frameDurations(std::move(frameDurations))
    , m_frameImages(std::move(frameImages))
{
}

ImageSequence::ImageSequence(std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    bool hasProviderKnownMetadata,
    const QSizeF &providerKnownLogicalSize,
    QVector<int> providerKnownFrameDurations,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability,
    QObject *parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Provider)
    , m_providerSessionFactory(std::move(providerSessionFactory))
    , m_hasProviderKnownMetadata(hasProviderKnownMetadata)
    , m_providerKnownLogicalSize(providerKnownLogicalSize)
    , m_providerKnownFrameDurations(std::move(providerKnownFrameDurations))
    , m_providerTimedPlaybackCapability(timedPlaybackCapability)
    , m_providerFrameSeekCapability(frameSeekCapability)
    , m_providerPositionSeekCapability(positionSeekCapability)
{
}

bool ImageSequence::isValid() const
{
    if (isProvider()) {
        return m_providerSessionFactory != nullptr;
    }

    return m_timingModel != TimingModel::None
        && m_logicalSize.isValid()
        && m_logicalSize.width() > 0.0
        && m_logicalSize.height() > 0.0
        && (isStill() || !m_frameDurations.isEmpty());
}

bool ImageSequence::isStill() const
{
    return m_timingModel == TimingModel::Still;
}

bool ImageSequence::isTimedList() const
{
    return m_timingModel == TimingModel::TimedList;
}

bool ImageSequence::isProvider() const
{
    return m_timingModel == TimingModel::Provider;
}

QSizeF ImageSequence::logicalSize() const
{
    return m_logicalSize;
}

int ImageSequence::frameCount() const
{
    if (isStill()) {
        return 1;
    }
    if (isTimedList()) {
        return m_frameDurations.size();
    }

    return -1;
}

int ImageSequence::totalDuration() const
{
    if (!isTimedList()) {
        return -1;
    }

    int total = 0;
    for (int duration : m_frameDurations) {
        total += duration;
    }
    return total;
}

int ImageSequence::frameStartPosition(int frame) const
{
    if (!isTimedList() || frame < 0 || frame >= m_frameDurations.size()) {
        return -1;
    }

    int position = 0;
    for (int index = 0; index < frame; ++index) {
        position += m_frameDurations.at(index);
    }
    return position;
}

int ImageSequence::frameIndexForPosition(int position) const
{
    if (!isTimedList() || position < 0 || position > totalDuration()) {
        return -1;
    }
    if (position == totalDuration()) {
        return m_frameDurations.size() - 1;
    }

    int frameStart = 0;
    for (int index = 0; index < m_frameDurations.size(); ++index) {
        const int frameEnd = frameStart + m_frameDurations.at(index);
        if (position >= frameStart && position < frameEnd) {
            return index;
        }
        frameStart = frameEnd;
    }

    return -1;
}

QImage ImageSequence::frameImage(int frame) const
{
    if (isStill() && frame == 0) {
        return m_stillImage;
    }
    if (isTimedList() && frame >= 0 && frame < m_frameImages.size()) {
        return m_frameImages.at(frame);
    }
    return {};
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
QImage ImageSequence::frameImageForTest(int frame) const
{
    return frameImage(frame);
}
#endif

ImageFrame::ImageFrame(QObject *parent)
    : QObject(parent)
{
}

ImageFrame::ImageFrame(const QImage &image, QObject *parent)
    : QObject(parent)
{
    if (!image.isNull() && image.width() > 0 && image.height() > 0) {
        m_logicalSize = QSizeF(image.width(), image.height());
        m_payloadByteSize = image.sizeInBytes();
        if (m_payloadByteSize <= ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
            m_image = image.copy();
        }
    }
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ImageFrame::ImageFrame(const QImage &image, qsizetype payloadByteSizeForTest, QObject *parent)
    : QObject(parent)
{
    if (!image.isNull() && image.width() > 0 && image.height() > 0) {
        m_logicalSize = QSizeF(image.width(), image.height());
        m_payloadByteSize = payloadByteSizeForTest;
        m_image = image.copy();
    }
}
#endif

bool ImageFrame::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0;
}

QSizeF ImageFrame::logicalSize() const
{
    return m_logicalSize;
}

qsizetype ImageFrame::payloadByteSize() const
{
    return m_payloadByteSize;
}

const QImage &ImageFrame::imagePayload() const
{
    return m_image;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
QImage ImageFrame::imageForTest() const
{
    return m_image;
}
#endif

TimedImageFrameList::TimedImageFrameList(QObject *parent)
    : QObject(parent)
{
}

int TimedImageFrameList::count() const
{
    return m_frameDurations.size();
}

QString TimedImageFrameList::errorString() const
{
    return m_errorString;
}

QString TimedImageFrameList::warningString() const
{
    return m_warningString;
}

bool TimedImageFrameList::appendFrame(ImageFrame *frame, int durationMilliseconds)
{
    if (!frame || !frame->isValid()) {
        setErrorString(QStringLiteral("ImageFrame is required"));
        return false;
    }
    if (durationMilliseconds <= 0) {
        setErrorString(QStringLiteral("frame duration must be positive"));
        return false;
    }
    if (durationMilliseconds > ImageSequenceLimits::maximumFrameDuration()) {
        setErrorString(QStringLiteral("frame duration exceeds maximumFrameDuration"));
        return false;
    }
    if (m_frameDurations.size() >= ImageSequenceLimits::maximumTimedListFrameCount()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumTimedListFrameCount"));
        return false;
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        setErrorString(limitViolation);
        return false;
    }

    if (!m_logicalSize.isValid()) {
        m_logicalSize = frame->logicalSize();
    } else if (m_logicalSize != frame->logicalSize()) {
        setErrorString(QStringLiteral("frame logical size must match the timed list logical size"));
        return false;
    }

    if (static_cast<qint64>(totalDuration()) + durationMilliseconds > ImageSequenceLimits::maximumTotalSequenceDuration()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumTotalSequenceDuration"));
        return false;
    }

    m_frameDurations.append(durationMilliseconds);
    m_frameImages.append(frame->imagePayload());
    if (!m_errorString.isEmpty()) {
        m_errorString.clear();
        emit diagnosticsChanged();
    }
    emit countChanged();
    return true;
}

void TimedImageFrameList::clear()
{
    if (m_frameDurations.isEmpty() && m_errorString.isEmpty() && m_warningString.isEmpty()) {
        return;
    }

    const bool shouldEmitCountChanged = !m_frameDurations.isEmpty();
    const bool shouldEmitDiagnosticsChanged = !m_errorString.isEmpty() || !m_warningString.isEmpty();
    m_logicalSize = {};
    m_frameDurations.clear();
    m_frameImages.clear();
    m_errorString.clear();
    m_warningString.clear();
    if (shouldEmitCountChanged) {
        emit countChanged();
    }
    if (shouldEmitDiagnosticsChanged) {
        emit diagnosticsChanged();
    }
}

bool TimedImageFrameList::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0 && !m_frameDurations.isEmpty();
}

QSizeF TimedImageFrameList::logicalSize() const
{
    return m_logicalSize;
}

QVector<int> TimedImageFrameList::frameDurations() const
{
    return m_frameDurations;
}

QVector<QImage> TimedImageFrameList::frameImages() const
{
    return m_frameImages;
}

int TimedImageFrameList::totalDuration() const
{
    int total = 0;
    for (int duration : m_frameDurations) {
        total += duration;
    }
    return total;
}

void TimedImageFrameList::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    emit diagnosticsChanged();
}

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject *parent)
    : QObject(parent)
{
}

std::shared_ptr<ImageSequenceProviderSessionFactory> ImageSequenceProviderAdapter::sessionFactory() const
{
    return {};
}

ImageSequenceProviderMetadata ImageSequenceProviderAdapter::knownMetadata() const
{
    return {};
}

ImageSequenceProviderAdapter::CapabilitySupport ImageSequenceProviderAdapter::timedPlaybackCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderAdapter::CapabilitySupport ImageSequenceProviderAdapter::frameSeekCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderAdapter::CapabilitySupport ImageSequenceProviderAdapter::positionSeekCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderRequestToken::ImageSequenceProviderRequestToken(quint64 id)
    : m_id(id)
{
}

quint64 ImageSequenceProviderRequestToken::id() const
{
    return m_id;
}

bool ImageSequenceProviderRequestToken::isValid() const
{
    return m_id != 0;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::still(const QSizeF &logicalSize)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_timingModel = TimingModel::Still;
    metadata.m_logicalSize = logicalSize;
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::fixedDurationFrames(const QSizeF &logicalSize, int frameCount, int frameDuration)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_timingModel = TimingModel::FixedDurationFrames;
    metadata.m_logicalSize = logicalSize;
    if (frameCount > 0) {
        metadata.m_frameDurations = QVector<int>(frameCount, frameDuration);
    }
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::timedFrameList(const QSizeF &logicalSize, QVector<int> frameDurations)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_timingModel = TimingModel::TimedFrameList;
    metadata.m_logicalSize = logicalSize;
    metadata.m_frameDurations = std::move(frameDurations);
    return metadata;
}

bool ImageSequenceProviderMetadata::isValid() const
{
    if (!m_logicalSize.isValid() || m_logicalSize.width() <= 0.0 || m_logicalSize.height() <= 0.0) {
        return false;
    }
    if (isStill()) {
        return true;
    }
    if (isTimedFrameList()) {
        return !m_frameDurations.isEmpty();
    }

    return false;
}

bool ImageSequenceProviderMetadata::isSpecified() const
{
    return m_timingModel != TimingModel::Invalid;
}

bool ImageSequenceProviderMetadata::isStill() const
{
    return m_timingModel == TimingModel::Still;
}

bool ImageSequenceProviderMetadata::isTimedFrameList() const
{
    return m_timingModel == TimingModel::FixedDurationFrames || m_timingModel == TimingModel::TimedFrameList;
}

QSizeF ImageSequenceProviderMetadata::logicalSize() const
{
    return m_logicalSize;
}

QVector<int> ImageSequenceProviderMetadata::frameDurations() const
{
    return m_frameDurations;
}

ImageSequenceProviderFrameMetadata ImageSequenceProviderFrameMetadata::stillFrame()
{
    ImageSequenceProviderFrameMetadata metadata;
    metadata.m_timingModel = TimingModel::Still;
    metadata.m_frame = 0;
    return metadata;
}

ImageSequenceProviderFrameMetadata ImageSequenceProviderFrameMetadata::timedFrame(int frame, int frameStartPosition, int frameDuration)
{
    ImageSequenceProviderFrameMetadata metadata;
    metadata.m_timingModel = TimingModel::TimedFrame;
    metadata.m_frame = frame;
    metadata.m_frameStartPosition = frameStartPosition;
    metadata.m_frameDuration = frameDuration;
    return metadata;
}

bool ImageSequenceProviderFrameMetadata::isValid() const
{
    if (isStillFrame()) {
        return m_frame == 0;
    }
    if (isTimedFrame()) {
        return m_frame >= 0 && m_frameStartPosition >= 0 && (m_frameDuration == -1 || m_frameDuration > 0);
    }

    return false;
}

bool ImageSequenceProviderFrameMetadata::isStillFrame() const
{
    return m_timingModel == TimingModel::Still;
}

bool ImageSequenceProviderFrameMetadata::isTimedFrame() const
{
    return m_timingModel == TimingModel::TimedFrame;
}

int ImageSequenceProviderFrameMetadata::frame() const
{
    return m_frame;
}

int ImageSequenceProviderFrameMetadata::frameStartPosition() const
{
    return m_frameStartPosition;
}

int ImageSequenceProviderFrameMetadata::frameDuration() const
{
    return m_frameDuration;
}

ImageSequenceProviderSession::ImageSequenceProviderSession(QObject *parent)
    : QObject(parent)
{
}

void ImageSequenceProviderSession::requestFrame(const ImageSequenceProviderRequestToken &, int)
{
}

void ImageSequenceProviderSession::requestPlayback(const ImageSequenceProviderRequestToken &token, int frame, int)
{
    requestFrame(token, frame);
}

void ImageSequenceProviderSession::cancelRequest(const ImageSequenceProviderRequestToken &)
{
}

void ImageSequenceProviderSession::close()
{
}

ImageSequenceFactoryResult::ImageSequenceFactoryResult(ImageSequence *sequence,
    FactoryOutcome outcome,
    QString errorString,
    QString warningString,
    QObject *parent)
    : QObject(parent)
    , m_sequence(sequence)
    , m_outcome(outcome)
    , m_errorString(std::move(errorString))
    , m_warningString(std::move(warningString))
{
}

ImageSequence *ImageSequenceFactoryResult::sequence() const
{
    return m_sequence;
}

ImageSequenceFactoryResult::FactoryOutcome ImageSequenceFactoryResult::outcome() const
{
    return m_outcome;
}

QString ImageSequenceFactoryResult::errorString() const
{
    return m_errorString;
}

QString ImageSequenceFactoryResult::warningString() const
{
    return m_warningString;
}

ImageSequenceFactory::ImageSequenceFactory(QObject *parent)
    : QObject(parent)
{
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromFrame(ImageFrame *frame)
{
    if (!frame) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageFrame is required"),
            {},
            this);
    }

    if (!frame->isValid()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageFrame must have a positive logical size"),
            {},
            this);
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            limitViolation,
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(frame->logicalSize(), frame->imagePayload(), this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromTimedFrameList(TimedImageFrameList *list)
{
    if (!list || !list->isValid()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("TimedImageFrameList must contain at least one frame"),
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(list->logicalSize(), list->frameDurations(), list->frameImages(), this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromProvider(ImageSequenceProviderAdapter *adapter)
{
    if (!adapter) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageSequenceProviderAdapter is required"),
            {},
            this);
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory = adapter->sessionFactory();
    if (!sessionFactory) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageSequenceProviderAdapter must provide a bounded session factory"),
            {},
            this);
    }

    const ImageSequenceProviderMetadata knownMetadata = adapter->knownMetadata();
    const QString metadataViolation = providerMetadataLimitViolation(knownMetadata);
    if (!metadataViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            metadataViolation,
            {},
            this);
    }

    const bool hasKnownMetadata = knownMetadata.isSpecified();
    if (hasKnownMetadata
        && (providerCapabilityContradictsMetadata(adapter->timedPlaybackCapability(), knownMetadata.isTimedFrameList())
            || providerCapabilityContradictsMetadata(adapter->frameSeekCapability(), true)
            || providerCapabilityContradictsMetadata(adapter->positionSeekCapability(), knownMetadata.isTimedFrameList()))) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("provider metadata contradicts declared capabilities"),
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(std::move(sessionFactory),
                                           hasKnownMetadata,
                                           hasKnownMetadata ? knownMetadata.logicalSize() : QSizeF(),
                                           hasKnownMetadata && knownMetadata.isTimedFrameList() ? knownMetadata.frameDurations() : QVector<int>(),
                                           adapter->timedPlaybackCapability(),
                                           adapter->frameSeekCapability(),
                                           adapter->positionSeekCapability(),
                                           this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceLimits::ImageSequenceLimits(QObject *parent)
    : QObject(parent)
{
}

int ImageSequenceLimits::getMaximumLogicalWidth() const
{
    return maximumLogicalWidth();
}

int ImageSequenceLimits::getMaximumLogicalHeight() const
{
    return maximumLogicalHeight();
}

qint64 ImageSequenceLimits::getMaximumPixelsPerFrame() const
{
    return maximumPixelsPerFrame();
}

qint64 ImageSequenceLimits::getMaximumPayloadBytesPerFrame() const
{
    return maximumPayloadBytesPerFrame();
}

int ImageSequenceLimits::getMaximumTimedListFrameCount() const
{
    return maximumTimedListFrameCount();
}

int ImageSequenceLimits::getMaximumFrameDuration() const
{
    return maximumFrameDuration();
}

int ImageSequenceLimits::getMaximumTotalSequenceDuration() const
{
    return maximumTotalSequenceDuration();
}

int ImageSequenceLimits::getMaximumDiagnosticStringLength() const
{
    return maximumDiagnosticStringLength();
}

int ImageSequenceLimits::maximumLogicalWidth()
{
    return minimumMaximumLogicalSide;
}

int ImageSequenceLimits::maximumLogicalHeight()
{
    return minimumMaximumLogicalSide;
}

qint64 ImageSequenceLimits::maximumPixelsPerFrame()
{
    return minimumMaximumPixelsPerFrame;
}

qint64 ImageSequenceLimits::maximumPayloadBytesPerFrame()
{
    return minimumMaximumPayloadBytesPerFrame;
}

int ImageSequenceLimits::maximumTimedListFrameCount()
{
    return minimumMaximumTimedListFrameCount;
}

int ImageSequenceLimits::maximumFrameDuration()
{
    return minimumMaximumDuration;
}

int ImageSequenceLimits::maximumTotalSequenceDuration()
{
    return minimumMaximumDuration;
}

int ImageSequenceLimits::maximumDiagnosticStringLength()
{
    return minimumMaximumDiagnosticStringLength;
}

ImageViewport::ImageViewport(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

ImageViewport::~ImageViewport()
{
    closeProviderSession();
}

ImageSequence *ImageViewport::sequence() const
{
    return m_sequence;
}

void ImageViewport::setSequence(ImageSequence *sequence)
{
    if (!m_sequence && !sequence) {
        return;
    }

    const DisplayStatus oldDisplayStatus = m_displayStatus;
    const PlaybackPhase oldPlaybackPhase = m_playbackPhase;
    const QString oldErrorString = m_errorString;
    const QString oldWarningString = m_warningString;
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    closeProviderSession();
    m_sequence = sequence;
    m_errorString.clear();
    m_warningString.clear();
    m_playbackPhase = PlaybackPhase::Stopped;
    m_stopPlaybackWhenRequestReady = false;
    m_providerPlaybackStartPending = false;
    m_providerMetadataReady = false;
    m_providerTimedMetadata = false;
    m_providerLogicalSize = {};
    m_providerFrameDurations.clear();
    m_pendingDisplayImage = {};
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;

    if (hasProviderSequence()) {
        if (m_sequence->m_hasProviderKnownMetadata) {
            m_providerMetadataReady = true;
            m_providerTimedMetadata = !m_sequence->m_providerKnownFrameDurations.isEmpty();
            m_providerLogicalSize = m_sequence->m_providerKnownLogicalSize;
            m_providerFrameDurations = m_sequence->m_providerKnownFrameDurations;
            m_currentFrame = 0;
            m_requestedPosition = m_providerTimedMetadata ? 0 : -1;
            m_playbackPosition = m_requestedPosition;
            m_latestNonPlaybackFrame = m_currentFrame;
            m_latestNonPlaybackPosition = m_requestedPosition;
        } else {
            m_currentFrame = -1;
            m_requestedPosition = -1;
            m_playbackPosition = -1;
            m_latestNonPlaybackFrame = -1;
            m_latestNonPlaybackPosition = -1;
        }
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        if (!openProviderSession()) {
            m_requestStatus = RequestStatus::Error;
            m_requestReason = RequestReason::ProviderFailure;
            m_errorString = QStringLiteral("provider session creation failed");
        }
    } else if (hasDisplayableSequence()) {
        m_currentFrame = 0;
        m_requestedPosition = hasTimedSequence() ? 0 : -1;
        m_playbackPosition = hasTimedSequence() ? 0 : -1;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        if (width() > 0.0 && height() > 0.0) {
            publishSequenceReadyState();
        } else {
            publishRenderWaitingState();
        }
    } else {
        m_currentFrame = -1;
        m_requestedPosition = -1;
        m_playbackPosition = -1;
        m_latestNonPlaybackFrame = -1;
        m_latestNonPlaybackPosition = -1;
        m_displayedFrame = -1;
        m_displayedPosition = -1;
        m_displayedImageSize = {};
        m_displayedImage = {};
        m_requestStatus = RequestStatus::NoRequest;
        m_requestReason = RequestReason::NoRequest;
        m_displayStatus = DisplayStatus::Empty;
        m_renderCommitPending = false;
        clearRenderFailureRetainedDisplay();
    }

    incrementRequestRevision();
    const bool displayValueChanged = m_displayStatus != oldDisplayStatus || m_displayStatus == DisplayStatus::Ready;
    if (displayValueChanged) {
        incrementDisplayRevision();
    }
    emit sequenceChanged();
    emit requestStateChanged();
    if (displayValueChanged) {
        emit displayStateChanged();
    }
    if (contentRect() != oldContentRect
        || visibleImageRect() != oldVisibleImageRect) {
        emit geometryStateChanged();
    }
    if (m_playbackPhase != oldPlaybackPhase) {
        emit playbackPhaseChanged();
    }
    if (m_errorString != oldErrorString || m_warningString != oldWarningString) {
        emit diagnosticsChanged();
    }
    update();
}

ImageViewport::RequestStatus ImageViewport::requestStatus() const
{
    return m_requestStatus;
}

ImageViewport::RequestReason ImageViewport::requestReason() const
{
    return m_requestReason;
}

ImageViewport::CommandReason ImageViewport::commandReason() const
{
    return m_commandReason;
}

ImageViewport::DisplayStatus ImageViewport::displayStatus() const
{
    return m_displayStatus;
}

ImageViewport::PlaybackPhase ImageViewport::playbackPhase() const
{
    return m_playbackPhase;
}

int ImageViewport::displayedFrame() const
{
    if (hasReadyDisplay()) {
        return m_displayedFrame;
    }

    return -1;
}

int ImageViewport::requestedFrame() const
{
    if (hasDisplayableSequence()) {
        return m_currentFrame;
    }

    return -1;
}

int ImageViewport::displayedPosition() const
{
    if (hasReadyDisplay()) {
        return m_displayedPosition;
    }

    return -1;
}

int ImageViewport::requestedPosition() const
{
    if (hasProviderSequence() && (m_providerTimedMetadata || m_requestedPosition >= 0)) {
        return m_requestedPosition;
    }
    if (hasTimedSequence()) {
        return m_requestedPosition;
    }

    return -1;
}

int ImageViewport::frameCount() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return m_providerTimedMetadata ? m_providerFrameDurations.size() : 1;
    }
    if (hasDisplayableSequence()) {
        return m_sequence->frameCount();
    }

    return -1;
}

int ImageViewport::totalDuration() const
{
    if (hasProviderSequence() && m_providerTimedMetadata) {
        int total = 0;
        for (int duration : m_providerFrameDurations) {
            total += duration;
        }
        return total;
    }
    if (hasTimedSequence()) {
        return m_sequence->totalDuration();
    }

    return -1;
}

QVariantMap ImageViewport::frameSeekBounds() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), m_providerTimedMetadata ? m_providerFrameDurations.size() - 1 : 0},
        };
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), m_sequence->frameCount() - 1},
        };
    }

    return invalidRange();
}

QVariantMap ImageViewport::positionSeekBounds() const
{
    if (hasProviderSequence() && m_providerTimedMetadata) {
        int total = 0;
        for (int duration : m_providerFrameDurations) {
            total += duration;
        }
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), total},
        };
    }
    if (hasTimedSequence()) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), m_sequence->totalDuration()},
        };
    }

    return invalidRange();
}

ImageViewport::TriState ImageViewport::timedPlaybackSupport() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return m_providerTimedMetadata ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(m_sequence->m_providerTimedPlaybackCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewport::TriState ImageViewport::frameSeekSupport() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return TriState::True;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(m_sequence->m_providerFrameSeekCapability);
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewport::TriState ImageViewport::positionSeekSupport() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return m_providerTimedMetadata ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(m_sequence->m_providerPositionSeekCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

QSizeF ImageViewport::displayedImageSize() const
{
    if (hasReadyDisplay()) {
        return m_displayedImageSize;
    }

    return QSizeF(0.0, 0.0);
}

QRectF ImageViewport::contentRect() const
{
    return currentContentRect();
}

QRectF ImageViewport::visibleImageRect() const
{
    if (!hasReadyDisplay() || itemBounds().isEmpty()) {
        return {};
    }

    const QRectF content = currentContentRect();
    if (content.isEmpty()) {
        return {};
    }

    const QRectF visibleItemRect = content.intersected(itemBounds());
    if (visibleItemRect.isEmpty()) {
        return {};
    }

    const QSizeF imageSize = currentImageSize();
    const double x = (visibleItemRect.x() - content.x()) / content.width() * imageSize.width();
    const double y = (visibleItemRect.y() - content.y()) / content.height() * imageSize.height();
    const double width = visibleItemRect.width() / content.width() * imageSize.width();
    const double height = visibleItemRect.height() / content.height() * imageSize.height();
    QRectF visibleImageRect(x, y, width, height);
    if (m_mirrorHorizontally) {
        visibleImageRect.moveLeft(imageSize.width() - visibleImageRect.right());
    }
    if (m_mirrorVertically) {
        visibleImageRect.moveTop(imageSize.height() - visibleImageRect.bottom());
    }
    return visibleImageRect;
}

uint ImageViewport::displayRevision() const
{
    return m_displayRevision;
}

uint ImageViewport::requestRevision() const
{
    return m_requestRevision;
}

uint ImageViewport::commandRevision() const
{
    return m_commandRevision;
}

QString ImageViewport::errorString() const
{
    return m_errorString;
}

QString ImageViewport::warningString() const
{
    return m_warningString;
}

ImageViewport::FillMode ImageViewport::fillMode() const
{
    return m_fillMode;
}

void ImageViewport::setFillMode(FillMode mode)
{
    if (!isValidFillMode(mode) || m_fillMode == mode) {
        return;
    }

    m_fillMode = mode;
    notifyPresentationChanged(true);
}

ImageViewport::HorizontalAlignment ImageViewport::horizontalAlignment() const
{
    return m_horizontalAlignment;
}

void ImageViewport::setHorizontalAlignment(HorizontalAlignment alignment)
{
    if (!isValidHorizontalAlignment(alignment) || m_horizontalAlignment == alignment) {
        return;
    }

    m_horizontalAlignment = alignment;
    notifyPresentationChanged(true);
}

ImageViewport::VerticalAlignment ImageViewport::verticalAlignment() const
{
    return m_verticalAlignment;
}

void ImageViewport::setVerticalAlignment(VerticalAlignment alignment)
{
    if (!isValidVerticalAlignment(alignment) || m_verticalAlignment == alignment) {
        return;
    }

    m_verticalAlignment = alignment;
    notifyPresentationChanged(true);
}

bool ImageViewport::smoothing() const
{
    return m_smoothing;
}

void ImageViewport::setSmoothing(bool smoothing)
{
    if (m_smoothing == smoothing) {
        return;
    }

    m_smoothing = smoothing;
    notifyPresentationChanged(false);
}

bool ImageViewport::mipmap() const
{
    return m_mipmap;
}

void ImageViewport::setMipmap(bool mipmap)
{
    if (m_mipmap == mipmap) {
        return;
    }

    m_mipmap = mipmap;
    notifyPresentationChanged(false);
}

bool ImageViewport::mirrorHorizontally() const
{
    return m_mirrorHorizontally;
}

void ImageViewport::setMirrorHorizontally(bool mirror)
{
    if (m_mirrorHorizontally == mirror) {
        return;
    }

    m_mirrorHorizontally = mirror;
    notifyPresentationChanged(true);
}

bool ImageViewport::mirrorVertically() const
{
    return m_mirrorVertically;
}

void ImageViewport::setMirrorVertically(bool mirror)
{
    if (m_mirrorVertically == mirror) {
        return;
    }

    m_mirrorVertically = mirror;
    notifyPresentationChanged(true);
}

ImageViewport::BackgroundMode ImageViewport::backgroundMode() const
{
    return m_backgroundMode;
}

void ImageViewport::setBackgroundMode(BackgroundMode mode)
{
    if (!isValidBackgroundMode(mode) || m_backgroundMode == mode) {
        return;
    }

    m_backgroundMode = mode;
    notifyPresentationChanged(false);
}

QColor ImageViewport::backgroundColor() const
{
    return m_backgroundColor;
}

void ImageViewport::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    notifyPresentationChanged(false);
}

double ImageViewport::zoom() const
{
    return m_zoom;
}

void ImageViewport::setZoom(double zoom)
{
    if (!isFinitePositive(zoom) || m_zoom == zoom) {
        return;
    }

    m_zoom = zoom;
    notifyPresentationChanged(true);
}

QPointF ImageViewport::pan() const
{
    return m_pan;
}

void ImageViewport::setPan(const QPointF &pan)
{
    if (!isFinitePoint(pan) || m_pan == pan) {
        return;
    }

    m_pan = pan;
    notifyPresentationChanged(true);
}

bool ImageViewport::looping() const
{
    return m_looping;
}

void ImageViewport::setLooping(bool looping)
{
    if (m_looping == looping) {
        return;
    }

    m_looping = looping;
    emit loopingChanged();
}

ImageViewport::CommandOutcome ImageViewport::clear()
{
    const bool sequenceValueChanged = m_sequence != nullptr;
    const bool requestChanged = hasActiveRequest() || m_sequence;
    const bool displayChanged = m_displayStatus != DisplayStatus::Empty || m_displayedImageSize.isValid();
    const bool playbackChanged = m_playbackPhase != PlaybackPhase::Stopped;
    const bool diagnosticsValueChanged = !m_errorString.isEmpty() || !m_warningString.isEmpty();
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    closeProviderSession();
    m_sequence = nullptr;
    m_currentFrame = -1;
    m_requestedPosition = -1;
    m_playbackPosition = -1;
    m_latestNonPlaybackFrame = -1;
    m_latestNonPlaybackPosition = -1;
    m_displayedFrame = -1;
    m_displayedPosition = -1;
    m_displayedImageSize = {};
    m_displayedImage = {};
    m_pendingDisplayImage = {};
    m_renderCommitPending = false;
    clearRenderFailureRetainedDisplay();
    m_requestStatus = RequestStatus::NoRequest;
    m_requestReason = RequestReason::NoRequest;
    m_displayStatus = DisplayStatus::Empty;
    m_playbackPhase = PlaybackPhase::Stopped;
    m_stopPlaybackWhenRequestReady = false;
    m_providerPlaybackStartPending = false;
    m_providerMetadataReady = false;
    m_providerTimedMetadata = false;
    m_providerLogicalSize = {};
    m_providerFrameDurations.clear();
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
    m_errorString.clear();
    m_warningString.clear();
    clearCommandDiagnosticForAcceptedCommand();
    if (requestChanged) {
        incrementRequestRevision();
    }
    if (displayChanged) {
        incrementDisplayRevision();
    }

    if (sequenceValueChanged) {
        emit sequenceChanged();
    }
    if (requestChanged) {
        emit requestStateChanged();
    }
    if (displayChanged) {
        emit displayStateChanged();
    }
    if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
        emit geometryStateChanged();
    }
    if (playbackChanged) {
        emit playbackPhaseChanged();
    }
    if (diagnosticsValueChanged) {
        emit diagnosticsChanged();
    }
    update();
    return CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ImageViewport::play()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }
    if (hasGenerationTerminalProviderFailure()) {
        setCommandDiagnostic(CommandReason::UnsupportedRequest);
        return CommandOutcome::Unsupported;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        clearCommandDiagnosticForAcceptedCommand();
        m_stopPlaybackWhenRequestReady = false;
        m_playbackPosition = m_requestedPosition >= 0 ? m_requestedPosition : providerFrameStartPosition(m_currentFrame);
        setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting : PlaybackPhase::Playing);
        return CommandOutcome::Accepted;
    }

    if (hasProviderSequence() && !m_providerMetadataReady && m_requestStatus == RequestStatus::Loading) {
        if (m_sequence->m_providerTimedPlaybackCapability == ImageSequenceProviderCapabilitySupport::DeclaredFalse) {
            setCommandDiagnostic(CommandReason::UnsupportedRequest);
            return CommandOutcome::Unsupported;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_stopPlaybackWhenRequestReady = false;
        m_providerPlaybackStartPending = true;
        m_currentFrame = -1;
        m_requestedPosition = -1;
        m_playbackPosition = -1;
        setPlaybackPhase(PlaybackPhase::Waiting);
        incrementRequestRevision();
        emit requestStateChanged();
        return CommandOutcome::Accepted;
    }

    if (hasTimedSequence()) {
        clearCommandDiagnosticForAcceptedCommand();
        m_stopPlaybackWhenRequestReady = false;
        m_playbackPosition = m_requestedPosition >= 0 ? m_requestedPosition : m_sequence->frameStartPosition(m_currentFrame);
        setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting : PlaybackPhase::Playing);
        return CommandOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return CommandOutcome::Unsupported;
}

ImageViewport::CommandOutcome ImageViewport::pause()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    clearCommandDiagnosticForAcceptedCommand();
    if (m_playbackPhase == PlaybackPhase::Playing || m_playbackPhase == PlaybackPhase::Waiting) {
        setPlaybackPhase(PlaybackPhase::Paused);
    }
    return CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ImageViewport::stop()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    clearCommandDiagnosticForAcceptedCommand();
    m_stopPlaybackWhenRequestReady = false;
    if (hasProviderSequence()
        && !m_providerMetadataReady
        && m_requestStatus == RequestStatus::Loading
        && m_playbackPhase == PlaybackPhase::Waiting
        && m_currentFrame < 0
        && m_requestedPosition < 0) {
        m_currentFrame = m_latestNonPlaybackFrame;
        m_requestedPosition = m_latestNonPlaybackPosition;
        m_playbackPosition = m_requestedPosition;
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        return CommandOutcome::Accepted;
    }
    if (hasProviderSequence() && m_providerTimedMetadata && m_activeProviderFrameFromPlayback) {
        if (m_providerSession) {
            m_providerSession->cancelRequest(m_activeProviderFrameToken);
        }
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        if (hasReadyDisplay()) {
            m_currentFrame = m_displayedFrame;
            m_requestedPosition = m_displayedPosition;
            m_playbackPosition = m_requestedPosition;
            m_requestStatus = RequestStatus::Ready;
            m_requestReason = RequestReason::Ready;
            m_displayStatus = DisplayStatus::Ready;
            const bool diagnosticsValueChanged = clearDiagnostics();
            setPlaybackPhase(PlaybackPhase::Stopped);
            incrementRequestRevision();
            incrementDisplayRevision();
            emit requestStateChanged();
            emit displayStateChanged();
            if (diagnosticsValueChanged) {
                emit diagnosticsChanged();
            }
            return CommandOutcome::Accepted;
        }
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        m_pendingDisplayImage = {};
        const bool diagnosticsValueChanged = clearDiagnostics();
        m_activeProviderFrameToken = nextProviderRequestToken();
        if (m_providerSession && m_currentFrame >= 0) {
            m_providerSession->requestFrame(m_activeProviderFrameToken, m_currentFrame);
        }
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        return CommandOutcome::Accepted;
    }
    if (hasTimedSequence()
        && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::RenderWaiting
        && m_playbackPhase == PlaybackPhase::Waiting
        && m_latestNonPlaybackFrame >= 0
        && m_currentFrame != m_latestNonPlaybackFrame) {
        const DisplayStatus oldDisplayStatus = m_displayStatus;
        m_currentFrame = m_latestNonPlaybackFrame;
        m_requestedPosition = m_latestNonPlaybackPosition;
        m_playbackPosition = m_requestedPosition;
        if (hasReadyDisplay() && m_displayedFrame == m_currentFrame && m_displayedPosition == m_requestedPosition) {
            m_requestStatus = RequestStatus::Ready;
            m_requestReason = RequestReason::Ready;
            m_displayStatus = DisplayStatus::Ready;
        } else {
            publishRenderWaitingState();
        }
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        if (m_displayStatus != oldDisplayStatus) {
            incrementDisplayRevision();
            emit displayStateChanged();
        }
        emit requestStateChanged();
        update();
        return CommandOutcome::Accepted;
    }
    setPlaybackPhase(PlaybackPhase::Stopped);
    return CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ImageViewport::seek(int frame)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }
    if (frame < 0) {
        setCommandDiagnostic(CommandReason::InvalidRequest);
        return CommandOutcome::Invalid;
    }
    if (hasGenerationTerminalProviderFailure()) {
        setCommandDiagnostic(CommandReason::UnsupportedRequest);
        return CommandOutcome::Unsupported;
    }

    if (hasDisplayableSequence()) {
        if (hasProviderSequence() && m_providerMetadataReady) {
            const int maximumFrame = m_providerTimedMetadata ? m_providerFrameDurations.size() - 1 : 0;
            if (frame > maximumFrame) {
                setCommandDiagnostic(CommandReason::InvalidRequest);
                return CommandOutcome::Invalid;
            }

            clearCommandDiagnosticForAcceptedCommand();
            m_providerPlaybackStartPending = false;
            m_currentFrame = frame;
            m_requestedPosition = providerFrameStartPosition(frame);
            m_playbackPosition = m_requestedPosition;
            m_latestNonPlaybackFrame = m_currentFrame;
            m_latestNonPlaybackPosition = m_requestedPosition;
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::ProviderWaiting;
            m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
            m_pendingDisplayImage = {};
            const bool diagnosticsValueChanged = clearDiagnostics();
            if (m_providerSession && m_activeProviderFrameToken.isValid()) {
                m_providerSession->cancelRequest(m_activeProviderFrameToken);
            }
            m_activeProviderFrameToken = nextProviderRequestToken();
            m_activeProviderFrameFromPlayback = false;
            if (m_providerSession) {
                m_providerSession->requestFrame(m_activeProviderFrameToken, frame);
            }
            if (m_playbackPhase == PlaybackPhase::Playing) {
                setPlaybackPhase(PlaybackPhase::Waiting);
            }
            incrementRequestRevision();
            incrementDisplayRevision();
            emit requestStateChanged();
            emit displayStateChanged();
            if (diagnosticsValueChanged) {
                emit diagnosticsChanged();
            }
            update();
            return CommandOutcome::Accepted;
        }

        if (hasProviderSequence() && !m_providerMetadataReady && m_requestStatus == RequestStatus::Loading) {
            if (m_sequence->m_providerFrameSeekCapability == ImageSequenceProviderCapabilitySupport::DeclaredFalse) {
                setCommandDiagnostic(CommandReason::UnsupportedRequest);
                return CommandOutcome::Unsupported;
            }

            clearCommandDiagnosticForAcceptedCommand();
            m_providerPlaybackStartPending = false;
            m_currentFrame = frame;
            m_requestedPosition = -1;
            m_playbackPosition = -1;
            m_latestNonPlaybackFrame = m_currentFrame;
            m_latestNonPlaybackPosition = m_requestedPosition;
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::ProviderWaiting;
            m_pendingDisplayImage = {};
            const bool diagnosticsValueChanged = clearDiagnostics();
            incrementRequestRevision();
            emit requestStateChanged();
            if (diagnosticsValueChanged) {
                emit diagnosticsChanged();
            }
            return CommandOutcome::Accepted;
        }

        if (frame < 0 || frame >= m_sequence->frameCount()) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_providerPlaybackStartPending = false;
        m_currentFrame = frame;
        m_requestedPosition = hasTimedSequence() ? m_sequence->frameStartPosition(frame) : -1;
        m_playbackPosition = m_requestedPosition;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        const QRectF oldContentRect = contentRect();
        const QRectF oldVisibleImageRect = visibleImageRect();
        const bool diagnosticsValueChanged = clearDiagnostics();
        publishAcceptedTargetState();
        if (m_playbackPhase == PlaybackPhase::Playing && m_requestStatus == RequestStatus::Loading) {
            setPlaybackPhase(PlaybackPhase::Waiting);
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
            emit geometryStateChanged();
        }
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        update();
        return CommandOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return CommandOutcome::Unsupported;
}

ImageViewport::CommandOutcome ImageViewport::seekToPosition(int milliseconds)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    if (milliseconds < 0) {
        setCommandDiagnostic(CommandReason::InvalidRequest);
        return CommandOutcome::Invalid;
    }
    if (hasGenerationTerminalProviderFailure()) {
        setCommandDiagnostic(CommandReason::UnsupportedRequest);
        return CommandOutcome::Unsupported;
    }

    if (hasProviderSequence() && !m_providerMetadataReady && m_requestStatus == RequestStatus::Loading) {
        if (m_sequence->m_providerPositionSeekCapability == ImageSequenceProviderCapabilitySupport::DeclaredFalse) {
            setCommandDiagnostic(CommandReason::UnsupportedRequest);
            return CommandOutcome::Unsupported;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_providerPlaybackStartPending = false;
        m_currentFrame = -1;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_pendingDisplayImage = {};
        const bool diagnosticsValueChanged = clearDiagnostics();
        incrementRequestRevision();
        emit requestStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        return CommandOutcome::Accepted;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        const int frame = providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_providerPlaybackStartPending = false;
        m_currentFrame = frame;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        m_pendingDisplayImage = {};
        const bool diagnosticsValueChanged = clearDiagnostics();
        if (m_providerSession && m_activeProviderFrameToken.isValid()) {
            m_providerSession->cancelRequest(m_activeProviderFrameToken);
        }
        m_activeProviderFrameToken = nextProviderRequestToken();
        m_activeProviderFrameFromPlayback = false;
        if (m_providerSession) {
            m_providerSession->requestFrame(m_activeProviderFrameToken, frame);
        }
        if (m_playbackPhase == PlaybackPhase::Playing) {
            setPlaybackPhase(PlaybackPhase::Waiting);
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        update();
        return CommandOutcome::Accepted;
    }

    if (hasTimedSequence()) {
        const int frame = m_sequence->frameIndexForPosition(milliseconds);
        if (frame < 0) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_providerPlaybackStartPending = false;
        m_currentFrame = frame;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        const QRectF oldContentRect = contentRect();
        const QRectF oldVisibleImageRect = visibleImageRect();
        const bool diagnosticsValueChanged = clearDiagnostics();
        publishAcceptedTargetState();
        if (m_playbackPhase == PlaybackPhase::Playing && m_requestStatus == RequestStatus::Loading) {
            setPlaybackPhase(PlaybackPhase::Waiting);
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
            emit geometryStateChanged();
        }
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        update();
        return CommandOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return CommandOutcome::Unsupported;
}

ImageViewport::CommandOutcome ImageViewport::resetView()
{
    const bool changed = m_zoom != 1.0 || m_pan != QPointF();
    m_zoom = 1.0;
    m_pan = {};
    if (changed) {
        notifyPresentationChanged(true);
    }
    clearCommandDiagnosticForAcceptedCommand();
    return CommandOutcome::Accepted;
}

QVariantMap ImageViewport::itemToImage(double x, double y) const
{
    if (!hasReadyDisplay() || !std::isfinite(x) || !std::isfinite(y)) {
        return invalidCoordinateResult();
    }

    const QRectF bounds = itemBounds();
    const QRectF content = currentContentRect();
    const QSizeF imageSize = currentImageSize();
    if (bounds.isEmpty() || content.isEmpty() || x < bounds.left() || y < bounds.top() || x >= bounds.right() || y >= bounds.bottom()) {
        return invalidCoordinateResult();
    }

    double imageX = (x - content.x()) / content.width() * imageSize.width();
    double imageY = (y - content.y()) / content.height() * imageSize.height();
    if (m_mirrorHorizontally) {
        imageX = imageSize.width() - imageX;
    }
    if (m_mirrorVertically) {
        imageY = imageSize.height() - imageY;
    }

    if (!containsVisibleImagePoint(imageX, imageY)) {
        return invalidCoordinateResult();
    }

    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("x"), imageX},
        {QStringLiteral("y"), imageY},
    };
}

QVariantMap ImageViewport::imageToItem(double x, double y) const
{
    if (!containsVisibleImagePoint(x, y)) {
        return invalidCoordinateResult();
    }

    const QRectF content = currentContentRect();
    const QSizeF imageSize = currentImageSize();
    double normalizedX = x;
    double normalizedY = y;
    if (m_mirrorHorizontally) {
        normalizedX = imageSize.width() - x;
    }
    if (m_mirrorVertically) {
        normalizedY = imageSize.height() - y;
    }

    const double itemX = content.x() + normalizedX / imageSize.width() * content.width();
    const double itemY = content.y() + normalizedY / imageSize.height() * content.height();
    if (!itemBounds().contains(QPointF(itemX, itemY))) {
        return invalidCoordinateResult();
    }

    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("x"), itemX},
        {QStringLiteral("y"), itemY},
    };
}

bool ImageViewport::containsVisibleImagePoint(double x, double y) const
{
    if (!hasReadyDisplay() || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    const QSizeF imageSize = currentImageSize();
    if (x < 0.0 || y < 0.0 || x >= imageSize.width() || y >= imageSize.height()) {
        return false;
    }

    const QRectF visible = visibleImageRect();
    return x >= visible.left()
        && y >= visible.top()
        && x < visible.right()
        && y < visible.bottom();
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewport::advancePlaybackForTest(int elapsedMilliseconds)
{
    if (m_playbackPhase != PlaybackPhase::Playing || elapsedMilliseconds <= 0) {
        return;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        const int duration = totalDuration();
        const int previousFrame = m_currentFrame;
        int nextPlaybackPosition = m_playbackPosition < 0 ? providerFrameStartPosition(m_currentFrame) : m_playbackPosition;
        nextPlaybackPosition += elapsedMilliseconds;

        int nextFrame = -1;
        int nextRequestedPosition = -1;
        if (nextPlaybackPosition >= duration) {
            if (m_looping) {
                const int wrappedPosition = duration > 0 ? nextPlaybackPosition % duration : 0;
                nextFrame = providerFrameIndexForPosition(wrappedPosition);
                if (nextFrame < 0) {
                    return;
                }
                nextPlaybackPosition = wrappedPosition;
                nextRequestedPosition = providerFrameStartPosition(nextFrame);
            } else {
                nextFrame = frameCount() - 1;
                nextRequestedPosition = providerFrameStartPosition(nextFrame);
                nextPlaybackPosition = duration;
                m_stopPlaybackWhenRequestReady = true;
            }
        } else {
            nextFrame = providerFrameIndexForPosition(nextPlaybackPosition);
            if (nextFrame < 0) {
                return;
            }
            nextRequestedPosition = providerFrameStartPosition(nextFrame);
        }

        m_playbackPosition = nextPlaybackPosition;
        if (nextFrame == previousFrame && m_requestStatus == RequestStatus::Ready) {
            if (m_stopPlaybackWhenRequestReady) {
                setPlaybackPhase(PlaybackPhase::Stopped);
                m_stopPlaybackWhenRequestReady = false;
            }
            return;
        }

        m_currentFrame = nextFrame;
        m_requestedPosition = nextRequestedPosition;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        m_pendingDisplayImage = {};
        const bool diagnosticsValueChanged = clearDiagnostics();
        m_activeProviderFrameToken = nextProviderRequestToken();
        m_activeProviderFrameFromPlayback = true;
        if (m_providerSession) {
            m_providerSession->requestPlayback(m_activeProviderFrameToken, nextFrame, nextRequestedPosition);
        }
        setPlaybackPhase(PlaybackPhase::Waiting);
        incrementRequestRevision();
        if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
            incrementDisplayRevision();
        }
        emit requestStateChanged();
        emit displayStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        update();
        return;
    }

    if (!hasTimedSequence()) {
        return;
    }

    const int totalDuration = m_sequence->totalDuration();
    const int previousFrame = m_currentFrame;
    int nextPlaybackPosition = m_playbackPosition < 0 ? m_sequence->frameStartPosition(m_currentFrame) : m_playbackPosition;
    nextPlaybackPosition += elapsedMilliseconds;

    if (nextPlaybackPosition >= totalDuration) {
        if (m_looping) {
            const int wrappedPosition = totalDuration > 0 ? nextPlaybackPosition % totalDuration : 0;
            const int wrappedFrame = m_sequence->frameIndexForPosition(wrappedPosition);
            if (wrappedFrame < 0) {
                return;
            }

            m_currentFrame = wrappedFrame;
            m_requestedPosition = m_sequence->frameStartPosition(wrappedFrame);
            m_playbackPosition = wrappedPosition;
            const QRectF oldContentRect = contentRect();
            const QRectF oldVisibleImageRect = visibleImageRect();
            publishAcceptedTargetState();
            setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting : PlaybackPhase::Playing);
            incrementRequestRevision();
            if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
                incrementDisplayRevision();
            }
            emit requestStateChanged();
            emit displayStateChanged();
            if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
                emit geometryStateChanged();
            }
            update();
            return;
        }

        const int finalFrame = m_sequence->frameCount() - 1;
        m_currentFrame = finalFrame;
        m_requestedPosition = m_sequence->frameStartPosition(finalFrame);
        m_playbackPosition = totalDuration;
        const QRectF oldContentRect = contentRect();
        const QRectF oldVisibleImageRect = visibleImageRect();
        publishAcceptedTargetState();
        m_stopPlaybackWhenRequestReady = m_requestStatus == RequestStatus::Loading;
        setPlaybackPhase(m_stopPlaybackWhenRequestReady ? PlaybackPhase::Waiting : PlaybackPhase::Stopped);
        incrementRequestRevision();
        if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
            incrementDisplayRevision();
        }
        emit requestStateChanged();
        emit displayStateChanged();
        if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
            emit geometryStateChanged();
        }
        update();
        return;
    }

    const int nextFrame = m_sequence->frameIndexForPosition(nextPlaybackPosition);
    if (nextFrame < 0) {
        return;
    }

    m_playbackPosition = nextPlaybackPosition;
    if (nextFrame == m_currentFrame) {
        return;
    }

    m_currentFrame = nextFrame;
    m_requestedPosition = m_sequence->frameStartPosition(nextFrame);
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    publishAcceptedTargetState();
    setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting : PlaybackPhase::Playing);
    incrementRequestRevision();
    incrementDisplayRevision();
    emit requestStateChanged();
    emit displayStateChanged();
    if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
        emit geometryStateChanged();
    }
    update();
}
#endif

QSGNode *ImageViewport::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;

    if (width() <= 0.0 || height() <= 0.0) {
        return nullptr;
    }

    const bool hasBackground = m_backgroundMode != BackgroundMode::Transparent;
    const QImage image = hasReadyDisplay() ? m_displayedImage : QImage();
    if (!hasBackground && image.isNull()) {
        return nullptr;
    }

    auto *root = new QSGNode;
    if (m_backgroundMode == BackgroundMode::SolidColor) {
        auto *background = new QSGSimpleRectNode(QRectF(0.0, 0.0, width(), height()), m_backgroundColor);
        root->appendChildNode(background);
    } else if (m_backgroundMode == BackgroundMode::Checkerboard) {
        constexpr double checkerboardTileSize = 8.0;
        const QColor lightSquare(238, 238, 238);
        const QColor darkSquare(204, 204, 204);
        for (double y = 0.0; y < height(); y += checkerboardTileSize) {
            for (double x = 0.0; x < width(); x += checkerboardTileSize) {
                const int column = static_cast<int>(x / checkerboardTileSize);
                const int row = static_cast<int>(y / checkerboardTileSize);
                const QColor color = ((row + column) % 2 == 0) ? lightSquare : darkSquare;
                const QRectF tile(x,
                    y,
                    std::min(checkerboardTileSize, width() - x),
                    std::min(checkerboardTileSize, height() - y));
                root->appendChildNode(new QSGSimpleRectNode(tile, color));
            }
        }
    }

    if (!image.isNull() && !window()) {
        delete root;
        if (m_displayStatus == DisplayStatus::Ready && m_renderCommitPending) {
            reportRenderFailure();
        }
        return nullptr;
    }

    if (!image.isNull() && window()) {
        QSGTexture *texture = window()->createTextureFromImage(image);
        QSGImageNode *imageNode = window()->createImageNode();
        if (texture && imageNode) {
            imageNode->setTexture(texture);
            imageNode->setOwnsTexture(true);
            imageNode->setRect(currentContentRect().intersected(itemBounds()));
            imageNode->setSourceRect(visibleImageRect());
            imageNode->setFiltering(m_smoothing ? QSGTexture::Linear : QSGTexture::Nearest);
            imageNode->setMipmapFiltering(m_mipmap ? QSGTexture::Linear : QSGTexture::None);
            QSGImageNode::TextureCoordinatesTransformMode transform = {};
            if (m_mirrorHorizontally) {
                transform |= QSGImageNode::MirrorHorizontally;
            }
            if (m_mirrorVertically) {
                transform |= QSGImageNode::MirrorVertically;
            }
            imageNode->setTextureCoordinatesTransform(transform);
            root->appendChildNode(imageNode);
        } else {
            delete texture;
            delete imageNode;
            delete root;
            if (m_displayStatus == DisplayStatus::Ready && m_renderCommitPending) {
                reportRenderFailure();
            }
            return nullptr;
        }
    }

    if (!image.isNull()) {
        m_renderCommitPending = false;
        clearRenderFailureRetainedDisplay();
    }
    return root;
}

void ImageViewport::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }

    if (hasDisplayableSequence()
        && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::RenderWaiting
        && newGeometry.width() > 0.0
        && newGeometry.height() > 0.0) {
        publishSequenceReadyState();
        if (m_playbackPhase == PlaybackPhase::Waiting) {
            setPlaybackPhase(m_stopPlaybackWhenRequestReady ? PlaybackPhase::Stopped : PlaybackPhase::Playing);
            m_stopPlaybackWhenRequestReady = false;
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
    } else if (hasReadyDisplay()) {
        incrementDisplayRevision();
    }

    if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
        emit geometryStateChanged();
    }
    update();
}

QVariantMap ImageViewport::invalidRange()
{
    return {
        {QStringLiteral("minimum"), -1},
        {QStringLiteral("maximum"), -1},
    };
}

QVariantMap ImageViewport::invalidCoordinateResult()
{
    return {
        {QStringLiteral("valid"), false},
        {QStringLiteral("x"), 0.0},
        {QStringLiteral("y"), 0.0},
    };
}

void ImageViewport::notifyPresentationChanged(bool affectsGeometry)
{
    incrementDisplayRevision();
    emit presentationChanged();
    if (affectsGeometry && hasReadyDisplay() && !itemBounds().isEmpty()) {
        emit geometryStateChanged();
    }
    update();
}

void ImageViewport::incrementDisplayRevision()
{
    ++m_displayRevision;
    emit displayRevisionChanged();
}

void ImageViewport::incrementRequestRevision()
{
    ++m_requestRevision;
    emit requestRevisionChanged();
}

void ImageViewport::setPlaybackPhase(PlaybackPhase phase)
{
    if (m_playbackPhase == phase) {
        return;
    }

    m_playbackPhase = phase;
    emit playbackPhaseChanged();
}

void ImageViewport::setCommandDiagnostic(CommandReason reason)
{
    m_commandReason = reason;
    ++m_commandRevision;
    emit commandRevisionChanged();
    emit commandStateChanged();
}

void ImageViewport::clearCommandDiagnosticForAcceptedCommand()
{
    if (m_commandReason == CommandReason::NoCommand) {
        return;
    }

    setCommandDiagnostic(CommandReason::NoCommand);
}

bool ImageViewport::clearDiagnostics()
{
    if (m_errorString.isEmpty() && m_warningString.isEmpty()) {
        return false;
    }

    m_errorString.clear();
    m_warningString.clear();
    return true;
}

ImageViewport::CommandOutcome ImageViewport::ignoredNoRequest()
{
    setCommandDiagnostic(CommandReason::IgnoredNoRequest);
    return CommandOutcome::IgnoredNoRequest;
}

bool ImageViewport::hasActiveRequest() const
{
    return m_requestStatus != RequestStatus::NoRequest;
}

bool ImageViewport::hasReadyDisplay() const
{
    return hasDisplayableSequence()
        && (m_displayStatus == DisplayStatus::Ready || m_displayStatus == DisplayStatus::Retained)
        && m_displayedImageSize.isValid()
        && m_displayedImageSize.width() > 0.0
        && m_displayedImageSize.height() > 0.0;
}

bool ImageViewport::hasDisplayableSequence() const
{
    return m_sequence && m_sequence->isValid();
}

bool ImageViewport::hasStillSequence() const
{
    return m_sequence && m_sequence->isStill();
}

bool ImageViewport::hasTimedSequence() const
{
    return m_sequence && m_sequence->isTimedList();
}

bool ImageViewport::hasProviderSequence() const
{
    return m_sequence && m_sequence->isProvider();
}

bool ImageViewport::hasGenerationTerminalProviderFailure() const
{
    return hasProviderSequence()
        && !m_providerSession
        && (m_requestStatus == RequestStatus::Unsupported || m_requestStatus == RequestStatus::Error);
}

QRectF ImageViewport::currentContentRect() const
{
    if (!hasReadyDisplay()) {
        return {};
    }

    const QRectF bounds = itemBounds();
    const QSizeF imageSize = currentImageSize();
    if (bounds.isEmpty() || imageSize.isEmpty()) {
        return {};
    }

    QSizeF placedSize;
    switch (m_fillMode) {
    case FillMode::Contain: {
        const double scale = std::min(bounds.width() / imageSize.width(), bounds.height() / imageSize.height());
        placedSize = imageSize * scale;
        break;
    }
    case FillMode::Cover: {
        const double scale = std::max(bounds.width() / imageSize.width(), bounds.height() / imageSize.height());
        placedSize = imageSize * scale;
        break;
    }
    case FillMode::Stretch:
        placedSize = bounds.size();
        break;
    case FillMode::Center:
        placedSize = imageSize;
        break;
    }

    double x = 0.0;
    if (m_horizontalAlignment == HorizontalAlignment::AlignHCenter) {
        x = (bounds.width() - placedSize.width()) / 2.0;
    } else if (m_horizontalAlignment == HorizontalAlignment::AlignRight) {
        x = bounds.width() - placedSize.width();
    }

    double y = 0.0;
    if (m_verticalAlignment == VerticalAlignment::AlignVCenter) {
        y = (bounds.height() - placedSize.height()) / 2.0;
    } else if (m_verticalAlignment == VerticalAlignment::AlignBottom) {
        y = bounds.height() - placedSize.height();
    }

    QRectF rect(x, y, placedSize.width(), placedSize.height());
    const QPointF center = rect.center();
    rect.setSize(rect.size() * m_zoom);
    rect.moveCenter(center + m_pan);
    return rect;
}

QRectF ImageViewport::itemBounds() const
{
    if (width() <= 0.0 || height() <= 0.0) {
        return {};
    }

    return QRectF(0.0, 0.0, width(), height());
}

QSizeF ImageViewport::currentImageSize() const
{
    if (!hasReadyDisplay()) {
        return {};
    }

    return m_displayedImageSize;
}

void ImageViewport::closeProviderSession()
{
    if (!m_providerSession) {
        return;
    }

    ImageSequenceProviderSession *session = m_providerSession;
    const ImageSequenceProviderRequestToken metadataToken = m_activeProviderMetadataToken;
    const ImageSequenceProviderRequestToken frameToken = m_activeProviderFrameToken;
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
    m_providerSession.clear();
    if (metadataToken.isValid()) {
        session->cancelRequest(metadataToken);
    }
    if (frameToken.isValid()) {
        session->cancelRequest(frameToken);
    }
    session->close();
    delete session;
}

bool ImageViewport::openProviderSession()
{
    if (!hasProviderSequence() || !m_sequence->m_providerSessionFactory) {
        return false;
    }

    m_providerSession = m_sequence->m_providerSessionFactory->createSession(this);
    if (!m_providerSession) {
        return false;
    }

    connect(m_providerSession,
        &ImageSequenceProviderSession::metadataReady,
        this,
        &ImageViewport::handleProviderMetadataReady,
        Qt::QueuedConnection);
    connect(m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageFrame *>(&ImageSequenceProviderSession::frameReady),
        this,
        &ImageViewport::handleProviderFrameReady,
        Qt::QueuedConnection);
    connect(m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageFrame *, const ImageSequenceProviderFrameMetadata &>(&ImageSequenceProviderSession::frameReady),
        this,
        &ImageViewport::handleProviderFrameReadyWithMetadata,
        Qt::QueuedConnection);
    connect(m_providerSession,
        &ImageSequenceProviderSession::providerWaiting,
        this,
        &ImageViewport::handleProviderWaiting,
        Qt::QueuedConnection);
    connect(m_providerSession,
        &ImageSequenceProviderSession::providerProgress,
        this,
        &ImageViewport::handleProviderProgress,
        Qt::QueuedConnection);
    connect(m_providerSession,
        &ImageSequenceProviderSession::endOfSequence,
        this,
        &ImageViewport::handleProviderEndOfSequence,
        Qt::QueuedConnection);
    connect(m_providerSession,
        &ImageSequenceProviderSession::providerFailed,
        this,
        &ImageViewport::handleProviderFailure,
        Qt::QueuedConnection);
    connect(m_providerSession,
        &ImageSequenceProviderSession::providerUnsupported,
        this,
        &ImageViewport::handleProviderUnsupported,
        Qt::QueuedConnection);
    connect(m_providerSession,
        &ImageSequenceProviderSession::providerCancelled,
        this,
        &ImageViewport::handleProviderCancellation,
        Qt::QueuedConnection);

    if (m_providerMetadataReady) {
        m_pendingDisplayImage = {};
        m_activeProviderFrameToken = nextProviderRequestToken();
        m_activeProviderFrameFromPlayback = false;
        m_providerSession->requestFrame(m_activeProviderFrameToken, m_currentFrame);
    } else {
        m_activeProviderMetadataToken = nextProviderRequestToken();
        m_providerSession->requestMetadata(m_activeProviderMetadataToken);
    }
    return true;
}

ImageSequenceProviderRequestToken ImageViewport::nextProviderRequestToken()
{
    ++m_nextProviderRequestToken;
    return ImageSequenceProviderRequestToken(m_nextProviderRequestToken);
}

void ImageViewport::handleProviderMetadataReady(const ImageSequenceProviderRequestToken &token, const ImageSequenceProviderMetadata &metadata)
{
    if (!hasProviderSequence()
        || !m_providerSession
        || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_activeProviderMetadataToken = {};

    const bool isStillMetadata = validateProviderStillMetadata(metadata);
    const bool isTimedMetadata = validateProviderTimedMetadata(metadata);
    if (!isStillMetadata && !isTimedMetadata) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider metadata is invalid");
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        closeProviderSession();
        return;
    }

    if (providerCapabilityContradictsMetadata(m_sequence->m_providerTimedPlaybackCapability, isTimedMetadata)
        || providerCapabilityContradictsMetadata(m_sequence->m_providerFrameSeekCapability, true)
        || providerCapabilityContradictsMetadata(m_sequence->m_providerPositionSeekCapability, isTimedMetadata)) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider metadata contradicts construction-time capabilities");
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        closeProviderSession();
        return;
    }

    m_providerMetadataReady = true;
    m_providerTimedMetadata = isTimedMetadata;
    m_providerLogicalSize = metadata.logicalSize();
    m_providerFrameDurations = isTimedMetadata ? metadata.frameDurations() : QVector<int>();
    const bool selectedFromPlaybackStart = m_providerPlaybackStartPending && m_currentFrame < 0 && m_requestedPosition < 0;
    int selectedFrame = m_currentFrame >= 0 ? m_currentFrame : 0;
    const int providerFrameCount = isTimedMetadata ? m_providerFrameDurations.size() : 1;
    const bool selectedFromPosition = m_currentFrame < 0 && m_requestedPosition >= 0;
    if (selectedFromPlaybackStart && !isTimedMetadata) {
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::UnsupportedRequest;
        const bool diagnosticsValueChanged = clearDiagnostics();
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        return;
    }
    if (selectedFromPosition) {
        if (!isTimedMetadata) {
            m_requestStatus = RequestStatus::Unsupported;
            m_requestReason = RequestReason::UnsupportedRequest;
            const bool diagnosticsValueChanged = clearDiagnostics();
            setPlaybackPhase(PlaybackPhase::Stopped);
            incrementRequestRevision();
            emit requestStateChanged();
            if (diagnosticsValueChanged) {
                emit diagnosticsChanged();
            }
            return;
        }
        selectedFrame = providerFrameIndexForPosition(m_requestedPosition);
    }
    if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
        m_currentFrame = selectedFrame;
        if (!selectedFromPosition) {
            m_requestedPosition = -1;
        }
        m_playbackPosition = -1;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::InvalidRequest;
        const bool diagnosticsValueChanged = clearDiagnostics();
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        return;
    }

    m_currentFrame = selectedFrame;
    if (!selectedFromPosition) {
        m_requestedPosition = isTimedMetadata ? providerFrameStartPosition(selectedFrame) : -1;
    }
    m_playbackPosition = m_requestedPosition;
    if (m_playbackPhase != PlaybackPhase::Waiting) {
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
    }
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    m_pendingDisplayImage = {};

    m_activeProviderFrameToken = nextProviderRequestToken();
    m_activeProviderFrameFromPlayback = selectedFromPlaybackStart;
    m_providerPlaybackStartPending = false;
    if (selectedFromPlaybackStart) {
        m_providerSession->requestPlayback(m_activeProviderFrameToken, selectedFrame, m_requestedPosition);
    } else {
        m_providerSession->requestFrame(m_activeProviderFrameToken, selectedFrame);
    }
    incrementRequestRevision();
    emit requestStateChanged();
}

void ImageViewport::handleProviderFrameReady(const ImageSequenceProviderRequestToken &token, ImageFrame *frame)
{
    handleProviderFrameReadyWithMetadata(token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewport::handleProviderFrameReadyWithMetadata(const ImageSequenceProviderRequestToken &token, ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata)
{
    if (!hasProviderSequence()
        || !m_providerSession
        || !m_activeProviderFrameToken.isValid()
        || token != m_activeProviderFrameToken) {
        return;
    }

    if (frame && m_providerMetadataReady && frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider frame payload exceeds maximumPayloadBytesPerFrame");
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        return;
    }

    if (!validateProviderFrame(frame, metadata)) {
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider frame payload is invalid");
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        return;
    }

    const bool diagnosticsValueChanged = clearDiagnostics();
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    publishAcceptedTargetState(frame->imagePayload());
    if (m_playbackPhase == PlaybackPhase::Waiting) {
        setPlaybackPhase(m_stopPlaybackWhenRequestReady ? PlaybackPhase::Stopped : PlaybackPhase::Playing);
        m_stopPlaybackWhenRequestReady = false;
    }
    incrementRequestRevision();
    incrementDisplayRevision();
    emit requestStateChanged();
    emit displayStateChanged();
    if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
        emit geometryStateChanged();
    }
    if (diagnosticsValueChanged) {
        emit diagnosticsChanged();
    }
    update();
}

void ImageViewport::handleProviderWaiting(const ImageSequenceProviderRequestToken &token)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    const bool activeMetadataToken = !m_providerMetadataReady
        && m_activeProviderMetadataToken.isValid()
        && token == m_activeProviderMetadataToken;
    const bool activeFrameToken = m_activeProviderFrameToken.isValid()
        && token == m_activeProviderFrameToken;
    if ((!activeMetadataToken && !activeFrameToken) || m_requestStatus != RequestStatus::Loading) {
        return;
    }

    if (m_requestReason == RequestReason::ProviderWaiting) {
        return;
    }

    m_requestReason = RequestReason::ProviderWaiting;
    incrementRequestRevision();
    emit requestStateChanged();
}

void ImageViewport::handleProviderProgress(const ImageSequenceProviderRequestToken &token, double progress)
{
    if (!std::isfinite(progress) || progress < 0.0 || progress > 1.0) {
        return;
    }

    handleProviderWaiting(token);
}

void ImageViewport::handleProviderEndOfSequence(const ImageSequenceProviderRequestToken &token)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    const bool activeMetadataToken = !m_providerMetadataReady
        && m_activeProviderMetadataToken.isValid()
        && token == m_activeProviderMetadataToken;
    const bool activeFrameToken = m_activeProviderFrameToken.isValid()
        && token == m_activeProviderFrameToken;
    if (!activeMetadataToken && !activeFrameToken) {
        return;
    }

    if (activeMetadataToken
        || !m_providerMetadataReady
        || !m_providerTimedMetadata
        || !m_activeProviderFrameFromPlayback) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider protocol violation");
        m_providerPlaybackStartPending = false;
        m_stopPlaybackWhenRequestReady = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        closeProviderSession();
        return;
    }

    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
    const bool diagnosticsValueChanged = clearDiagnostics();

    int selectedFrame = 0;
    int selectedPosition = 0;
    if (m_looping) {
        m_stopPlaybackWhenRequestReady = false;
        m_playbackPosition = 0;
    } else {
        selectedFrame = frameCount() - 1;
        selectedPosition = providerFrameStartPosition(selectedFrame);
        m_playbackPosition = totalDuration();
        m_stopPlaybackWhenRequestReady = true;
    }

    m_currentFrame = selectedFrame;
    m_requestedPosition = selectedPosition;

    if (!m_looping && hasReadyDisplay() && m_displayedFrame == selectedFrame) {
        m_requestStatus = RequestStatus::Ready;
        m_requestReason = RequestReason::Ready;
        m_displayStatus = DisplayStatus::Ready;
        setPlaybackPhase(PlaybackPhase::Stopped);
        m_stopPlaybackWhenRequestReady = false;
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        if (diagnosticsValueChanged) {
            emit diagnosticsChanged();
        }
        update();
        return;
    }

    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    m_pendingDisplayImage = {};
    m_activeProviderFrameToken = nextProviderRequestToken();
    m_activeProviderFrameFromPlayback = true;
    if (m_providerSession) {
        m_providerSession->requestPlayback(m_activeProviderFrameToken, selectedFrame, selectedPosition);
    }
    setPlaybackPhase(PlaybackPhase::Waiting);
    incrementRequestRevision();
    incrementDisplayRevision();
    emit requestStateChanged();
    emit displayStateChanged();
    if (diagnosticsValueChanged) {
        emit diagnosticsChanged();
    }
    update();
}

void ImageViewport::handleProviderFailure(const ImageSequenceProviderRequestToken &token, const QString &diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken) {
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::ProviderFailure;
        m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider failure"));
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        return;
    }

    if (m_providerMetadataReady
        || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::ProviderFailure;
    m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider failure"));
    m_providerPlaybackStartPending = false;
    setPlaybackPhase(PlaybackPhase::Stopped);
    incrementRequestRevision();
    emit requestStateChanged();
    emit diagnosticsChanged();
    closeProviderSession();
}

void ImageViewport::handleProviderUnsupported(const ImageSequenceProviderRequestToken &token, const QString &diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken) {
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider unsupported"));
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        return;
    }

    if (m_providerMetadataReady
        || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_requestStatus = RequestStatus::Unsupported;
    m_requestReason = RequestReason::UnsupportedRequest;
    m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider unsupported"));
    m_providerPlaybackStartPending = false;
    setPlaybackPhase(PlaybackPhase::Stopped);
    incrementRequestRevision();
    emit requestStateChanged();
    emit diagnosticsChanged();
    closeProviderSession();
}

void ImageViewport::handleProviderCancellation(const ImageSequenceProviderRequestToken &token, const QString &diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken) {
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::ProviderFailure;
        m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider cancelled request"));
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        return;
    }

    if (m_providerMetadataReady
        || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::ProviderFailure;
    m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider cancelled request"));
    m_providerPlaybackStartPending = false;
    setPlaybackPhase(PlaybackPhase::Stopped);
    incrementRequestRevision();
    emit requestStateChanged();
    emit diagnosticsChanged();
    closeProviderSession();
}

bool ImageViewport::validateProviderStillMetadata(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isStill() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    if (!isAdmittedLogicalSizeComponent(size.width(), ImageSequenceLimits::maximumLogicalWidth())
        || !isAdmittedLogicalSizeComponent(size.height(), ImageSequenceLimits::maximumLogicalHeight())) {
        return false;
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    return width * height <= ImageSequenceLimits::maximumPixelsPerFrame();
}

bool ImageViewport::validateProviderTimedMetadata(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isTimedFrameList() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    if (!isAdmittedLogicalSizeComponent(size.width(), ImageSequenceLimits::maximumLogicalWidth())
        || !isAdmittedLogicalSizeComponent(size.height(), ImageSequenceLimits::maximumLogicalHeight())) {
        return false;
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return false;
    }

    const QVector<int> durations = metadata.frameDurations();
    if (durations.isEmpty() || durations.size() > ImageSequenceLimits::maximumTimedListFrameCount()) {
        return false;
    }

    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration <= 0 || duration > ImageSequenceLimits::maximumFrameDuration()) {
            return false;
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalSequenceDuration()) {
            return false;
        }
    }

    return true;
}

bool ImageViewport::validateProviderFrame(ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata) const
{
    if (!frame
        || !frame->isValid()
        || !m_providerMetadataReady
        || frame->logicalSize() != m_providerLogicalSize
        || frame->payloadByteSize() <= 0
        || frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()
        || !metadata.isValid()) {
        return false;
    }

    if (m_providerTimedMetadata) {
        if (!metadata.isTimedFrame() || metadata.frame() != m_currentFrame) {
            return false;
        }
        if (metadata.frameStartPosition() != providerFrameStartPosition(m_currentFrame)) {
            return false;
        }
        if (metadata.frameDuration() != -1 && metadata.frameDuration() != m_providerFrameDurations.at(m_currentFrame)) {
            return false;
        }
        return true;
    }

    return metadata.isStillFrame() && metadata.frame() == 0;
}

int ImageViewport::providerFrameStartPosition(int frame) const
{
    if (!m_providerTimedMetadata || frame < 0 || frame >= m_providerFrameDurations.size()) {
        return -1;
    }

    int position = 0;
    for (int index = 0; index < frame; ++index) {
        position += m_providerFrameDurations.at(index);
    }
    return position;
}

int ImageViewport::providerFrameIndexForPosition(int position) const
{
    if (!m_providerTimedMetadata || position < 0 || position > totalDuration()) {
        return -1;
    }
    if (position == totalDuration()) {
        return m_providerFrameDurations.size() - 1;
    }

    int frameStart = 0;
    for (int index = 0; index < m_providerFrameDurations.size(); ++index) {
        const int frameEnd = frameStart + m_providerFrameDurations.at(index);
        if (position >= frameStart && position < frameEnd) {
            return index;
        }
        frameStart = frameEnd;
    }

    return -1;
}

QString ImageViewport::boundedDiagnostic(const QString &diagnostic, const QString &fallback)
{
    const QString selected = redactDiagnosticDetails(diagnostic.isEmpty() ? fallback : diagnostic);
    const auto scalars = selected.toUcs4();
    const int maximumLength = ImageSequenceLimits::maximumDiagnosticStringLength();
    if (scalars.size() <= maximumLength) {
        return selected;
    }
    QString bounded;
    bounded.reserve(selected.size());
    for (int i = 0; i < maximumLength; ++i) {
        const char32_t scalar = static_cast<char32_t>(scalars.at(i));
        bounded += QString::fromUcs4(&scalar, 1);
    }
    return bounded;
}

void ImageViewport::reportRenderFailure()
{
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    const DisplayStatus oldDisplayStatus = m_displayStatus;

    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::RenderFailure;
    m_renderCommitPending = false;
    if (m_renderFailureRetainedDisplayValid) {
        m_displayStatus = DisplayStatus::Retained;
        m_displayedFrame = m_renderFailureRetainedFrame;
        m_displayedPosition = m_renderFailureRetainedPosition;
        m_displayedImageSize = m_renderFailureRetainedImageSize;
        m_displayedImage = m_renderFailureRetainedImage;
    } else {
        m_displayStatus = DisplayStatus::Empty;
        m_displayedFrame = -1;
        m_displayedPosition = -1;
        m_displayedImageSize = {};
        m_displayedImage = {};
    }
    m_pendingDisplayImage = {};
    clearRenderFailureRetainedDisplay();
    m_errorString = QStringLiteral("render commit failed");
    setPlaybackPhase(PlaybackPhase::Stopped);

    incrementRequestRevision();
    incrementDisplayRevision();
    emit requestStateChanged();
    if (m_displayStatus != oldDisplayStatus) {
        emit displayStateChanged();
    }
    if (contentRect() != oldContentRect || visibleImageRect() != oldVisibleImageRect) {
        emit geometryStateChanged();
    }
    emit diagnosticsChanged();
}

void ImageViewport::captureRenderFailureRetainedDisplay()
{
    if (!hasReadyDisplay()) {
        clearRenderFailureRetainedDisplay();
        return;
    }

    m_renderFailureRetainedDisplayValid = true;
    m_renderFailureRetainedFrame = m_displayedFrame;
    m_renderFailureRetainedPosition = m_displayedPosition;
    m_renderFailureRetainedImageSize = m_displayedImageSize;
    m_renderFailureRetainedImage = m_displayedImage;
}

void ImageViewport::clearRenderFailureRetainedDisplay()
{
    m_renderFailureRetainedDisplayValid = false;
    m_renderFailureRetainedFrame = -1;
    m_renderFailureRetainedPosition = -1;
    m_renderFailureRetainedImageSize = {};
    m_renderFailureRetainedImage = {};
}

void ImageViewport::publishAcceptedTargetState(const QImage &providerImage)
{
    if (itemBounds().isEmpty()) {
        if (hasProviderSequence() && !providerImage.isNull()) {
            m_pendingDisplayImage = providerImage;
        }
        publishRenderWaitingState();
    } else {
        publishSequenceReadyState(providerImage);
    }
}

void ImageViewport::publishSequenceReadyState(const QImage &providerImage)
{
    captureRenderFailureRetainedDisplay();
    m_requestStatus = RequestStatus::Ready;
    m_requestReason = RequestReason::Ready;
    m_displayStatus = DisplayStatus::Ready;
    m_renderCommitPending = true;
    m_displayedFrame = m_currentFrame;
    if (hasProviderSequence()) {
        m_displayedPosition = providerFrameStartPosition(m_currentFrame);
    } else {
        m_displayedPosition = hasTimedSequence() ? m_sequence->frameStartPosition(m_currentFrame) : -1;
    }
    m_displayedImageSize = hasProviderSequence() ? m_providerLogicalSize : m_sequence->logicalSize();
    if (hasProviderSequence()) {
        if (!providerImage.isNull()) {
            m_displayedImage = providerImage;
        } else if (!m_pendingDisplayImage.isNull()) {
            m_displayedImage = m_pendingDisplayImage;
        }
        m_pendingDisplayImage = {};
    } else {
        m_displayedImage = m_sequence ? m_sequence->frameImage(m_displayedFrame) : QImage();
        m_pendingDisplayImage = {};
    }
}

void ImageViewport::publishRenderWaitingState()
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::RenderWaiting;
    m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    m_renderCommitPending = false;
}
