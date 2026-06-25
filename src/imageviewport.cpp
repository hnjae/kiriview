#include "imageviewport.h"

#include <QtQuick/QSGNode>

#include <cmath>

namespace {

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QVariantMap invalidMappingResult()
{
    return {
        {QStringLiteral("valid"), false},
        {QStringLiteral("point"), QPointF(0.0, 0.0)},
    };
}

}

ImageSequence::ImageSequence(QObject *parent)
    : QObject(parent)
{
}

ImageFrame::ImageFrame(QObject *parent)
    : QObject(parent)
{
}

TimedImageFrame::TimedImageFrame(QObject *parent)
    : QObject(parent)
{
}

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject *parent)
    : QObject(parent)
{
}

ImageSequenceFactory::ImageSequenceFactory(QObject *parent)
    : QObject(parent)
{
}

ImageSequence *ImageSequenceFactory::fromImage(ImageFrame *)
{
    return nullptr;
}

ImageSequence *ImageSequenceFactory::fromFrames(const QList<TimedImageFrame *> &)
{
    return nullptr;
}

ImageSequence *ImageSequenceFactory::fromProvider(ImageSequenceProviderAdapter *)
{
    return nullptr;
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

    m_sequence = sequence;
    emit sequenceChanged();
}

bool ImageViewport::frameCountKnown() const
{
    return false;
}

int ImageViewport::frameCount() const
{
    return 0;
}

bool ImageViewport::durationKnown() const
{
    return false;
}

double ImageViewport::duration() const
{
    return 0.0;
}

bool ImageViewport::canSeek() const
{
    return false;
}

bool ImageViewport::canSeekByFrame() const
{
    return false;
}

bool ImageViewport::canSeekByPosition() const
{
    return false;
}

bool ImageViewport::streaming() const
{
    return false;
}

int ImageViewport::requestedFrame() const
{
    return m_requestedFrame;
}

void ImageViewport::setRequestedFrame(int frame)
{
    if (frame < 0) {
        setUnsupportedRequest(InvalidRequest);
        return;
    }

    setUnsupportedRequest(UnsupportedRequest);
}

int ImageViewport::displayedFrame() const
{
    return -1;
}

double ImageViewport::requestedPosition() const
{
    return -1.0;
}

double ImageViewport::displayedPosition() const
{
    return -1.0;
}

ImageViewport::PlaybackState ImageViewport::playbackState() const
{
    return m_playbackState;
}

double ImageViewport::speed() const
{
    return m_speed;
}

void ImageViewport::setSpeed(double speed)
{
    if (!isFinitePositive(speed)) {
        setWarningString(QStringLiteral("speed must be finite and greater than zero"));
        return;
    }

    if (qFuzzyCompare(m_speed, speed)) {
        return;
    }

    m_speed = speed;
    emit speedChanged();
}

ImageViewport::LoopMode ImageViewport::loopMode() const
{
    return m_loopMode;
}

void ImageViewport::setLoopMode(LoopMode mode)
{
    if (m_loopMode == mode) {
        return;
    }

    m_loopMode = mode;
    emit loopModeChanged();
}

int ImageViewport::loopCount() const
{
    return m_loopCount;
}

void ImageViewport::setLoopCount(int count)
{
    if (count <= 0) {
        setWarningString(QStringLiteral("loopCount must be greater than zero"));
        return;
    }

    if (m_loopCount == count) {
        return;
    }

    m_loopCount = count;
    emit loopCountChanged();
}

int ImageViewport::completedLoops() const
{
    return 0;
}

ImageViewport::RequestStatus ImageViewport::requestStatus() const
{
    return m_requestStatus;
}

ImageViewport::RequestStatusReason ImageViewport::requestStatusReason() const
{
    return m_requestStatusReason;
}

ImageViewport::DisplayStatus ImageViewport::displayStatus() const
{
    return Empty;
}

bool ImageViewport::hasDisplayableFrame() const
{
    return false;
}

bool ImageViewport::displayedBelongsToCurrentSequence() const
{
    return false;
}

int ImageViewport::displayRevision() const
{
    return 0;
}

QString ImageViewport::displayedSnapshotToken() const
{
    return {};
}

QString ImageViewport::errorString() const
{
    return {};
}

QString ImageViewport::warningString() const
{
    return m_warningString;
}

ImageViewport::RetentionPolicy ImageViewport::retentionPolicy() const
{
    return m_retentionPolicy;
}

void ImageViewport::setRetentionPolicy(RetentionPolicy policy)
{
    if (m_retentionPolicy == policy) {
        return;
    }

    m_retentionPolicy = policy;
    emit retentionPolicyChanged();
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

QRectF ImageViewport::contentRect() const
{
    return {};
}

QRectF ImageViewport::visibleImageRect() const
{
    return {};
}

double ImageViewport::paintedWidth() const
{
    return contentRect().width();
}

double ImageViewport::paintedHeight() const
{
    return contentRect().height();
}

double ImageViewport::zoom() const
{
    return m_zoom;
}

void ImageViewport::setZoom(double zoom)
{
    if (!isFinitePositive(zoom)) {
        setWarningString(QStringLiteral("zoom must be finite and greater than zero"));
        return;
    }

    if (qFuzzyCompare(m_zoom, zoom)) {
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
    if (!isFinitePoint(pan)) {
        setWarningString(QStringLiteral("pan coordinates must be finite"));
        return;
    }

    if (m_pan == pan) {
        return;
    }

    m_pan = pan;
    notifyPresentationChanged(true);
}

bool ImageViewport::smooth() const
{
    return m_smooth;
}

void ImageViewport::setSmooth(bool smooth)
{
    if (m_smooth == smooth) {
        return;
    }

    m_smooth = smooth;
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

bool ImageViewport::mirror() const
{
    return m_mirror;
}

void ImageViewport::setMirror(bool mirror)
{
    if (m_mirror == mirror) {
        return;
    }

    m_mirror = mirror;
    notifyPresentationChanged(true);
}

bool ImageViewport::mirrorVertically() const
{
    return m_mirrorVertically;
}

void ImageViewport::setMirrorVertically(bool mirrorVertically)
{
    if (m_mirrorVertically == mirrorVertically) {
        return;
    }

    m_mirrorVertically = mirrorVertically;
    notifyPresentationChanged(true);
}

ImageViewport::OrientationPolicy ImageViewport::orientationPolicy() const
{
    return m_orientationPolicy;
}

void ImageViewport::setOrientationPolicy(OrientationPolicy policy)
{
    if (m_orientationPolicy == policy) {
        return;
    }

    m_orientationPolicy = policy;
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

ImageViewport::ColorPolicy ImageViewport::colorPolicy() const
{
    return m_colorPolicy;
}

void ImageViewport::setColorPolicy(ColorPolicy policy)
{
    if (m_colorPolicy == policy) {
        return;
    }

    m_colorPolicy = policy;
    notifyPresentationChanged(false);
}

void ImageViewport::play()
{
    if (!m_sequence || m_playbackState == Playing) {
        return;
    }

    m_playbackState = Playing;
    emit playbackStateChanged();
}

void ImageViewport::pause()
{
    if (m_playbackState != Playing) {
        return;
    }

    m_playbackState = Paused;
    emit playbackStateChanged();
}

void ImageViewport::stop()
{
    if (m_playbackState == Stopped) {
        return;
    }

    m_playbackState = Stopped;
    emit playbackStateChanged();
}

ImageViewport::RequestOutcome ImageViewport::seek(int frame)
{
    if (frame < 0) {
        setUnsupportedRequest(InvalidRequest);
        return RequestOutcome::OutcomeInvalid;
    }

    setUnsupportedRequest(UnsupportedRequest);
    return RequestOutcome::OutcomeUnsupported;
}

ImageViewport::RequestOutcome ImageViewport::seekToPosition(double milliseconds)
{
    if (!std::isfinite(milliseconds) || milliseconds < 0.0) {
        setUnsupportedRequest(InvalidRequest);
        return RequestOutcome::OutcomeInvalid;
    }

    setUnsupportedRequest(UnsupportedRequest);
    return RequestOutcome::OutcomeUnsupported;
}

void ImageViewport::clear()
{
    const bool hadSequence = m_sequence;
    m_sequence = nullptr;
    m_requestedFrame = -1;
    m_playbackState = Stopped;
    m_requestStatus = NoRequest;
    m_requestStatusReason = None;
    m_warningString.clear();

    if (hadSequence) {
        emit sequenceChanged();
    }
    emit requestedFrameChanged();
    emit playbackStateChanged();
    emit requestStatusChanged();
    emit diagnosticsChanged();
    emit geometryStateChanged();
    update();
}

QVariantMap ImageViewport::mapItemToImage(const QPointF &) const
{
    return invalidMappingResult();
}

QVariantMap ImageViewport::mapImageToItem(const QPointF &) const
{
    return invalidMappingResult();
}

bool ImageViewport::containsVisibleImagePoint(const QPointF &) const
{
    return false;
}

QSGNode *ImageViewport::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;
    return nullptr;
}

void ImageViewport::notifyPresentationChanged(bool affectsGeometry)
{
    emit presentationChanged();
    if (affectsGeometry) {
        emit geometryStateChanged();
    }
    update();
}

void ImageViewport::setUnsupportedRequest(RequestStatusReason reason)
{
    const bool statusChanged = m_requestStatus != RequestUnsupported || m_requestStatusReason != reason;
    m_requestStatus = RequestUnsupported;
    m_requestStatusReason = reason;

    if (statusChanged) {
        emit requestStatusChanged();
    }
}

void ImageViewport::setWarningString(const QString &warning)
{
    if (m_warningString == warning) {
        return;
    }

    m_warningString = warning;
    emit diagnosticsChanged();
}
