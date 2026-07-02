if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "INPUT and OUTPUT are required")
endif()

file(READ "${INPUT}" installed_public_header)

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
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    void acknowledgeRenderCommitForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
#endif
]=])
    string(REPLACE "${private_probe_block}" "" installed_public_header "${installed_public_header}")
endforeach()

file(WRITE "${OUTPUT}" "${installed_public_header}")
