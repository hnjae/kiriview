#include "imageviewport.h"

#include <QtQuick/QSGNode>

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
    if (frame.payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        return QStringLiteral("ImageFrame exceeds maximumPayloadBytesPerFrame");
    }

    return {};
}

}

ImageSequence::ImageSequence(QObject *parent)
    : QObject(parent)
{
}

ImageSequence::ImageSequence(const QSizeF &logicalSize, QObject *parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Still)
    , m_logicalSize(logicalSize)
{
}

ImageSequence::ImageSequence(const QSizeF &logicalSize, QVector<int> frameDurations, QObject *parent)
    : QObject(parent)
    , m_timingModel(TimingModel::TimedList)
    , m_logicalSize(logicalSize)
    , m_frameDurations(std::move(frameDurations))
{
}

ImageSequence::ImageSequence(std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory, QObject *parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Provider)
    , m_providerSessionFactory(std::move(providerSessionFactory))
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
    }
}

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

    if (static_cast<qint64>(m_payloadByteSize) + frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumPayloadBytesPerFrame"));
        return false;
    }
    if (static_cast<qint64>(totalDuration()) + durationMilliseconds > ImageSequenceLimits::maximumTotalSequenceDuration()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumTotalSequenceDuration"));
        return false;
    }

    m_frameDurations.append(durationMilliseconds);
    m_payloadByteSize += frame->payloadByteSize();
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

    m_logicalSize = {};
    m_frameDurations.clear();
    m_payloadByteSize = 0;
    m_errorString.clear();
    m_warningString.clear();
    emit countChanged();
    emit diagnosticsChanged();
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

qsizetype TimedImageFrameList::payloadByteSize() const
{
    return m_payloadByteSize;
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

bool ImageSequenceProviderMetadata::isValid() const
{
    return isStill()
        && m_logicalSize.isValid()
        && m_logicalSize.width() > 0.0
        && m_logicalSize.height() > 0.0;
}

bool ImageSequenceProviderMetadata::isStill() const
{
    return m_timingModel == TimingModel::Still;
}

QSizeF ImageSequenceProviderMetadata::logicalSize() const
{
    return m_logicalSize;
}

ImageSequenceProviderSession::ImageSequenceProviderSession(QObject *parent)
    : QObject(parent)
{
}

void ImageSequenceProviderSession::requestFrame(const ImageSequenceProviderRequestToken &, int)
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

    return new ImageSequenceFactoryResult(new ImageSequence(frame->logicalSize(), this),
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

    return new ImageSequenceFactoryResult(new ImageSequence(list->logicalSize(), list->frameDurations(), this),
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

    return new ImageSequenceFactoryResult(new ImageSequence(std::move(sessionFactory), this),
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
    if (m_sequence == sequence) {
        return;
    }

    const DisplayStatus oldDisplayStatus = m_displayStatus;
    closeProviderSession();
    m_sequence = sequence;
    m_errorString.clear();
    m_warningString.clear();
    m_playbackPhase = PlaybackPhase::Stopped;
    m_stopPlaybackWhenRequestReady = false;
    m_providerMetadataReady = false;
    m_providerLogicalSize = {};
    m_activeProviderMetadataToken = {};

    if (hasProviderSequence()) {
        m_currentFrame = -1;
        m_requestedPosition = -1;
        m_playbackPosition = -1;
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
        if (width() > 0.0 && height() > 0.0) {
            publishSequenceReadyState();
        } else {
            publishRenderWaitingState();
        }
    } else {
        m_currentFrame = -1;
        m_requestedPosition = -1;
        m_playbackPosition = -1;
        m_requestStatus = RequestStatus::NoRequest;
        m_requestReason = RequestReason::NoRequest;
        m_displayStatus = DisplayStatus::Empty;
    }

    incrementRequestRevision();
    if (m_displayStatus != oldDisplayStatus) {
        incrementDisplayRevision();
    }
    emit sequenceChanged();
    emit requestStateChanged();
    emit displayStateChanged();
    emit geometryStateChanged();
    emit playbackPhaseChanged();
    emit diagnosticsChanged();
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
    if (hasTimedSequence()) {
        return m_requestedPosition;
    }

    return -1;
}

int ImageViewport::frameCount() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return 1;
    }
    if (hasDisplayableSequence()) {
        return m_sequence->frameCount();
    }

    return -1;
}

int ImageViewport::totalDuration() const
{
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
            {QStringLiteral("maximum"), 0},
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
        return TriState::False;
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
    if (hasStillSequence() || hasTimedSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewport::TriState ImageViewport::positionSeekSupport() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return TriState::False;
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
    return QRectF(x, y, width, height);
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
    if (m_fillMode == mode) {
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
    if (m_horizontalAlignment == alignment) {
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
    if (m_verticalAlignment == alignment) {
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
    if (m_backgroundMode == mode) {
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
    if (!isFinitePositive(zoom) || qFuzzyCompare(m_zoom, zoom)) {
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
    closeProviderSession();
    m_sequence = nullptr;
    m_currentFrame = -1;
    m_requestedPosition = -1;
    m_playbackPosition = -1;
    m_displayedFrame = -1;
    m_displayedPosition = -1;
    m_displayedImageSize = {};
    m_requestStatus = RequestStatus::NoRequest;
    m_requestReason = RequestReason::NoRequest;
    m_displayStatus = DisplayStatus::Empty;
    m_playbackPhase = PlaybackPhase::Stopped;
    m_stopPlaybackWhenRequestReady = false;
    m_providerMetadataReady = false;
    m_providerLogicalSize = {};
    m_activeProviderMetadataToken = {};
    m_errorString.clear();
    m_warningString.clear();
    clearCommandDiagnosticForAcceptedCommand();
    incrementRequestRevision();
    incrementDisplayRevision();

    emit sequenceChanged();
    emit requestStateChanged();
    emit displayStateChanged();
    emit playbackPhaseChanged();
    emit diagnosticsChanged();
    update();
    return CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ImageViewport::play()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
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
    setPlaybackPhase(PlaybackPhase::Stopped);
    return CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ImageViewport::seek(int frame)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    if (hasDisplayableSequence()) {
        if (frame < 0 || frame >= m_sequence->frameCount()) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_currentFrame = frame;
        m_requestedPosition = hasTimedSequence() ? m_sequence->frameStartPosition(frame) : -1;
        m_playbackPosition = m_requestedPosition;
        publishAcceptedTargetState();
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        emit geometryStateChanged();
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

    if (hasTimedSequence()) {
        const int frame = m_sequence->frameIndexForPosition(milliseconds);
        if (frame < 0) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_currentFrame = frame;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        publishAcceptedTargetState();
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        emit geometryStateChanged();
        update();
        return CommandOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return CommandOutcome::Unsupported;
}

ImageViewport::CommandOutcome ImageViewport::resetView()
{
    const bool changed = !qFuzzyCompare(m_zoom, 1.0) || m_pan != QPointF();
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
    if (!hasTimedSequence() || m_playbackPhase != PlaybackPhase::Playing || elapsedMilliseconds <= 0) {
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
            publishAcceptedTargetState();
            setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting : PlaybackPhase::Playing);
            incrementRequestRevision();
            if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
                incrementDisplayRevision();
            }
            emit requestStateChanged();
            emit displayStateChanged();
            emit geometryStateChanged();
            update();
            return;
        }

        const int finalFrame = m_sequence->frameCount() - 1;
        m_currentFrame = finalFrame;
        m_requestedPosition = m_sequence->frameStartPosition(finalFrame);
        m_playbackPosition = totalDuration;
        publishAcceptedTargetState();
        m_stopPlaybackWhenRequestReady = m_requestStatus == RequestStatus::Loading;
        setPlaybackPhase(m_stopPlaybackWhenRequestReady ? PlaybackPhase::Waiting : PlaybackPhase::Stopped);
        incrementRequestRevision();
        if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
            incrementDisplayRevision();
        }
        emit requestStateChanged();
        emit displayStateChanged();
        emit geometryStateChanged();
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
    publishAcceptedTargetState();
    setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting : PlaybackPhase::Playing);
    incrementRequestRevision();
    incrementDisplayRevision();
    emit requestStateChanged();
    emit displayStateChanged();
    emit geometryStateChanged();
    update();
}
#endif

QSGNode *ImageViewport::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;
    return nullptr;
}

void ImageViewport::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }

    if (hasDisplayableSequence() && m_requestStatus == RequestStatus::Loading && newGeometry.width() > 0.0 && newGeometry.height() > 0.0) {
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

    emit geometryStateChanged();
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
    if (affectsGeometry) {
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
    m_providerSession.clear();
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
        &ImageViewport::handleProviderMetadataReady);

    m_activeProviderMetadataToken = nextProviderRequestToken();
    m_providerSession->requestMetadata(m_activeProviderMetadataToken);
    return true;
}

ImageSequenceProviderRequestToken ImageViewport::nextProviderRequestToken()
{
    ++m_nextProviderRequestToken;
    return ImageSequenceProviderRequestToken(m_nextProviderRequestToken);
}

void ImageViewport::handleProviderMetadataReady(const ImageSequenceProviderRequestToken &token, const ImageSequenceProviderMetadata &metadata)
{
    if (!hasProviderSequence() || !m_providerSession || token != m_activeProviderMetadataToken) {
        return;
    }

    if (!validateProviderStillMetadata(metadata)) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider metadata is invalid");
        incrementRequestRevision();
        emit requestStateChanged();
        emit diagnosticsChanged();
        closeProviderSession();
        return;
    }

    m_providerMetadataReady = true;
    m_providerLogicalSize = metadata.logicalSize();
    m_currentFrame = 0;
    m_requestedPosition = -1;
    m_playbackPosition = -1;
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;

    ImageSequenceProviderRequestToken frameToken = nextProviderRequestToken();
    m_providerSession->requestFrame(frameToken, 0);
    incrementRequestRevision();
    emit requestStateChanged();
}

bool ImageViewport::validateProviderStillMetadata(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isStill() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    return width <= ImageSequenceLimits::maximumLogicalWidth()
        && height <= ImageSequenceLimits::maximumLogicalHeight()
        && width * height <= ImageSequenceLimits::maximumPixelsPerFrame();
}

void ImageViewport::publishAcceptedTargetState()
{
    if (itemBounds().isEmpty()) {
        publishRenderWaitingState();
    } else {
        publishSequenceReadyState();
    }
}

void ImageViewport::publishSequenceReadyState()
{
    m_requestStatus = RequestStatus::Ready;
    m_requestReason = RequestReason::Ready;
    m_displayStatus = DisplayStatus::Ready;
    m_displayedFrame = m_currentFrame;
    m_displayedPosition = hasTimedSequence() ? m_sequence->frameStartPosition(m_currentFrame) : -1;
    m_displayedImageSize = m_sequence->logicalSize();
}

void ImageViewport::publishRenderWaitingState()
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::RenderWaiting;
    m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
}
