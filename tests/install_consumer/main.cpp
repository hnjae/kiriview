#include <imageviewport.h>

#include <QImage>

#include <memory>

class ConsumerSession final : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void requestMetadata(const ImageSequenceProviderRequestToken &) override
    {
    }
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
};

int main()
{
    [[maybe_unused]] const auto knownTrue = ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue;
    [[maybe_unused]] const auto knownFalse = ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse;

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
