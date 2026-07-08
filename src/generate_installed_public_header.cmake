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
class ProviderRequestTokenPrivateAccess;
class RevisionTokenPrivateAccess;
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

string(REPLACE "    friend class ImageViewportInternal::ImageFramePrivateAccess;\n" ""
               installed_public_header "${installed_public_header}")

string(REPLACE "    friend class ImageViewportInternal::ProviderRequestTokenPrivateAccess;\n" ""
               installed_public_header "${installed_public_header}")

string(REPLACE "    friend class ImageViewportInternal::RevisionTokenPrivateAccess;\n" ""
               installed_public_header "${installed_public_header}")

string(
    REPLACE "    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;\n"
            "" installed_public_header "${installed_public_header}")

file(WRITE "${OUTPUT}" "${installed_public_header}")
