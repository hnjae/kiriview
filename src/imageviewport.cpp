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

}

ImageSequence::ImageSequence(QObject *parent)
    : QObject(parent)
{
}

ImageSequence::ImageSequence(const QSizeF &logicalSize, QObject *parent)
    : QObject(parent)
    , m_logicalSize(logicalSize)
{
}

bool ImageSequence::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0;
}

QSizeF ImageSequence::logicalSize() const
{
    return m_logicalSize;
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

TimedImageFrameList::TimedImageFrameList(QObject *parent)
    : QObject(parent)
{
}

int TimedImageFrameList::count() const
{
    return 0;
}

void TimedImageFrameList::clear()
{
}

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject *parent)
    : QObject(parent)
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

    return new ImageSequenceFactoryResult(new ImageSequence(frame->logicalSize(), this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromTimedFrameList(TimedImageFrameList *list)
{
    if (!list || list->count() <= 0) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("TimedImageFrameList must contain at least one frame"),
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(this),
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

    return new ImageSequenceFactoryResult(new ImageSequence(this),
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
    m_sequence = sequence;
    m_errorString.clear();
    m_warningString.clear();
    m_playbackPhase = PlaybackPhase::Stopped;

    if (hasStillSequence()) {
        if (width() > 0.0 && height() > 0.0) {
            publishStillSequenceState();
        } else {
            publishRenderWaitingStillState();
        }
    } else {
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
    if (hasReadyStillDisplay()) {
        return 0;
    }

    return -1;
}

int ImageViewport::requestedFrame() const
{
    if (hasStillSequence()) {
        return 0;
    }

    return -1;
}

int ImageViewport::displayedPosition() const
{
    return -1;
}

int ImageViewport::requestedPosition() const
{
    return -1;
}

int ImageViewport::frameCount() const
{
    if (hasStillSequence()) {
        return 1;
    }

    return -1;
}

int ImageViewport::totalDuration() const
{
    return -1;
}

QVariantMap ImageViewport::frameSeekBounds() const
{
    if (hasStillSequence()) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), 0},
        };
    }

    return invalidRange();
}

QVariantMap ImageViewport::positionSeekBounds() const
{
    return invalidRange();
}

ImageViewport::TriState ImageViewport::timedPlaybackSupport() const
{
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewport::TriState ImageViewport::frameSeekSupport() const
{
    if (hasStillSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewport::TriState ImageViewport::positionSeekSupport() const
{
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

QSizeF ImageViewport::displayedImageSize() const
{
    if (hasReadyStillDisplay()) {
        return currentImageSize();
    }

    return QSizeF(0.0, 0.0);
}

QRectF ImageViewport::contentRect() const
{
    return currentContentRect();
}

QRectF ImageViewport::visibleImageRect() const
{
    if (!hasReadyStillDisplay() || itemBounds().isEmpty()) {
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

ImageViewport::RequestOutcome ImageViewport::clear()
{
    m_sequence = nullptr;
    m_requestStatus = RequestStatus::NoRequest;
    m_requestReason = RequestReason::NoRequest;
    m_displayStatus = DisplayStatus::Empty;
    m_playbackPhase = PlaybackPhase::Stopped;
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
    return RequestOutcome::Accepted;
}

ImageViewport::RequestOutcome ImageViewport::play()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return RequestOutcome::Unsupported;
}

ImageViewport::RequestOutcome ImageViewport::pause()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    clearCommandDiagnosticForAcceptedCommand();
    if (m_playbackPhase == PlaybackPhase::Playing || m_playbackPhase == PlaybackPhase::Waiting) {
        m_playbackPhase = PlaybackPhase::Paused;
        emit playbackPhaseChanged();
    }
    return RequestOutcome::Accepted;
}

ImageViewport::RequestOutcome ImageViewport::stop()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    clearCommandDiagnosticForAcceptedCommand();
    if (m_playbackPhase != PlaybackPhase::Stopped) {
        m_playbackPhase = PlaybackPhase::Stopped;
        emit playbackPhaseChanged();
    }
    return RequestOutcome::Accepted;
}

ImageViewport::RequestOutcome ImageViewport::seek(int frame)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    if (hasStillSequence()) {
        if (frame != 0) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return RequestOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        publishStillSequenceState();
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
        emit geometryStateChanged();
        update();
        return RequestOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return RequestOutcome::Unsupported;
}

ImageViewport::RequestOutcome ImageViewport::seekToPosition(int milliseconds)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    if (milliseconds < 0) {
        setCommandDiagnostic(CommandReason::InvalidRequest);
        return RequestOutcome::Invalid;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return RequestOutcome::Unsupported;
}

ImageViewport::RequestOutcome ImageViewport::resetView()
{
    const bool changed = !qFuzzyCompare(m_zoom, 1.0) || m_pan != QPointF();
    m_zoom = 1.0;
    m_pan = {};
    if (changed) {
        notifyPresentationChanged(true);
    }
    clearCommandDiagnosticForAcceptedCommand();
    return RequestOutcome::Accepted;
}

QVariantMap ImageViewport::itemToImage(double x, double y) const
{
    if (!hasReadyStillDisplay() || !std::isfinite(x) || !std::isfinite(y)) {
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
    if (!hasReadyStillDisplay() || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    const QSizeF imageSize = currentImageSize();
    if (x < 0.0 || y < 0.0 || x >= imageSize.width() || y >= imageSize.height()) {
        return false;
    }

    return visibleImageRect().contains(QPointF(x, y));
}

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

    if (hasStillSequence() && m_requestStatus == RequestStatus::Loading && newGeometry.width() > 0.0 && newGeometry.height() > 0.0) {
        publishStillSequenceState();
        incrementRequestRevision();
        incrementDisplayRevision();
        emit requestStateChanged();
        emit displayStateChanged();
    } else if (hasReadyStillDisplay()) {
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

ImageViewport::RequestOutcome ImageViewport::ignoredNoRequest()
{
    setCommandDiagnostic(CommandReason::IgnoredNoRequest);
    return RequestOutcome::IgnoredNoRequest;
}

bool ImageViewport::hasActiveRequest() const
{
    return m_requestStatus != RequestStatus::NoRequest;
}

bool ImageViewport::hasReadyStillDisplay() const
{
    return hasStillSequence() && m_displayStatus == DisplayStatus::Ready;
}

bool ImageViewport::hasStillSequence() const
{
    return m_sequence && m_sequence->isValid();
}

QRectF ImageViewport::currentContentRect() const
{
    if (!hasReadyStillDisplay()) {
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
    if (!hasStillSequence()) {
        return {};
    }

    return m_sequence->logicalSize();
}

void ImageViewport::publishStillSequenceState()
{
    m_requestStatus = RequestStatus::Ready;
    m_requestReason = RequestReason::Ready;
    m_displayStatus = DisplayStatus::Ready;
}

void ImageViewport::publishRenderWaitingStillState()
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::RenderWaiting;
    m_displayStatus = DisplayStatus::Empty;
}
