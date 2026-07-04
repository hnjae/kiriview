if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "INPUT and OUTPUT are required")
endif()

file(READ "${INPUT}" installed_public_header)

string(
    REPLACE
        [=[
namespace ImageViewportInternal {
class ImageFramePrivateAccess;
class ImageSequenceData;
class ImageSequencePrivateAccess;
}

]=]
        ""
        installed_public_header
        "${installed_public_header}")

string(
    REPLACE
        [=[
private:
    explicit ImageSequence(
        std::unique_ptr<ImageViewportInternal::ImageSequenceData> data, QObject* parent = nullptr);

    std::unique_ptr<ImageViewportInternal::ImageSequenceData> d;

    friend class ImageViewportInternal::ImageSequencePrivateAccess;
]=]
        [=[
private:
    class Data;
    std::unique_ptr<Data> d;
]=]
        installed_public_header
        "${installed_public_header}")

string(
    REPLACE
        "    friend class ImageViewportInternal::ImageFramePrivateAccess;\n"
        ""
        installed_public_header
        "${installed_public_header}")

string(
    REPLACE
        "    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;\n"
        ""
        installed_public_header
        "${installed_public_header}")

foreach(
    private_probe_block IN
    ITEMS
        [=[
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
public:
    QImage frameImageForTest(int frame) const;

private:
#endif
]=]
        [=[
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    ImageFrame(const QImage& image, qsizetype payloadByteSizeForTest, QObject* parent = nullptr);
#endif
]=]
        [=[
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    QImage imageForTest() const;
#endif
]=]
        [=[
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTest(quint64 token);
    void setNextProviderRequestTokenForTest(ImageViewport::PageRole role, quint64 token);
    void failNextProviderCommandDeliveryForTest(ImageViewport::PageRole role);
    void useSynchronousProviderExecutorForTest();
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    quint64 secondaryPendingRenderPayloadIdForTest() const;
    void acknowledgeRenderCommitForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderCommitForTest(quint64 generation, quint64 requestId,
        quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId);
    void acknowledgeRenderFailureForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(ImageViewport::PageRole failedRole,
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
#endif
]=])
    string(REPLACE "${private_probe_block}" "" installed_public_header "${installed_public_header}")
endforeach()

file(WRITE "${OUTPUT}" "${installed_public_header}")
