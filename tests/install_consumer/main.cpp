#include <imageviewport.h>

#include <QImage>

#include <memory>

class ConsumerSession final : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0)));
    }

    void requestFrame(const ImageSequenceProviderRequestToken &token, int frame) override
    {
        QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        m_frame = std::make_unique<ImageFrame>(image);
        emit frameReady(token, m_frame.get(), ImageSequenceProviderFrameMetadata::timedFrame(frame, 0, 100));
    }

private:
    std::unique_ptr<ImageFrame> m_frame;
};

class ConsumerSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        return new ConsumerSession(parent);
    }
};

class ConsumerAdapter final : public ImageSequenceProviderAdapter
{
public:
    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return std::make_shared<ConsumerSessionFactory>();
    }

    ImageSequenceProviderMetadata knownMetadata() const override
    {
        return ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(2.0, 2.0), 2, 100);
    }

    CapabilitySupport timedPlaybackCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }

    CapabilitySupport frameSeekCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }

    CapabilitySupport positionSeekCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }
};

int main()
{
    [[maybe_unused]] const auto knownTrue = ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue;
    [[maybe_unused]] const auto knownFalse = ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse;
    ImageSequenceProviderRequestToken token(1);
    if (!token.isValid() || token.id() != 1 || token != ImageSequenceProviderRequestToken(1)) {
        return 1;
    }

    const ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::timedFrameList(QSizeF(2.0, 2.0), {100, 200});
    if (!metadata.isSpecified() || !metadata.isValid() || metadata.isStill() || !metadata.isTimedFrameList()) {
        return 1;
    }
    if (metadata.logicalSize() != QSizeF(2.0, 2.0) || metadata.frameDurations().size() != 2) {
        return 1;
    }

    const ImageSequenceProviderFrameMetadata frameMetadata = ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 200);
    if (!frameMetadata.isValid() || !frameMetadata.isTimedFrame() || frameMetadata.frame() != 1 || frameMetadata.frameStartPosition() != 100) {
        return 1;
    }

    ImageSequenceFactory factory;
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    std::unique_ptr<ImageSequenceFactoryResult> stillResult(factory.fromFrame(image));
    if (!stillResult || !stillResult->sequence()) {
        return 1;
    }
    if (stillResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    QVector<QImage> timedImages;
    timedImages.append(image);
    timedImages.append(image);
    QVector<int> timedDurations{100, 200};
    std::unique_ptr<ImageSequenceFactoryResult> timedResult(factory.fromTimedFrameList(timedImages, timedDurations));
    if (!timedResult || !timedResult->sequence()) {
        return 1;
    }
    if (timedResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    TimedImageFrameList builder;
    if (!builder.appendFrame(image, 100)) {
        return 1;
    }
    if (builder.count() != 1) {
        return 1;
    }

    ConsumerAdapter adapter;
    std::unique_ptr<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    if (!result || !result->sequence()) {
        return 1;
    }
    return result->outcome() == ImageSequenceFactoryResult::FactoryOutcome::Created ? 0 : 1;
}
