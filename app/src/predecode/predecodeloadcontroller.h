// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODELOADCONTROLLER_H
#define KIRIVIEW_PREDECODELOADCONTROLLER_H

#include "decoding/imagedecodedependencies.h"
#include "predecodeactivedecodestore.h"
#include "predecodedimage.h"
#include "predecodeloadstate.h"

#include <QtGlobal>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
class PredecodeLoadController final
{
public:
    PredecodeLoadController(
        QObject* parent, ImageDecodeDependencies decodeDependencies, qsizetype cacheByteBudget);
    ~PredecodeLoadController();
    Q_DISABLE_COPY_MOVE(PredecodeLoadController)

    void cacheDisplayedImages(const std::vector<DisplayedPredecodeImage>& images);
    void clearWindow();
    void startWindowLoads(const PredecodeLoadWindow& window);
    void retireBackgroundLoad(const DisplayedImageLocation& location);
    void supersedeBackgroundWindow();
    void cancelBackgroundWork();
    void clear();
    std::optional<PredecodedImage> findPredecodedImage(
        const DisplayedImageLocation& location) const;

private:
    struct Lifetime final
    {
    };

    void startNextLoads();
    bool startLoad(PredecodeLoadStart load);
    void finishLoadError(const ImageDecodeRequest& request, const ImageDataLoadError& error);
    void finishDecode(const ImageDecodeRequest& request, const DecodedImageResult& result);
    void finishRetirement(const ImageDecodeRequest& request);

    QObject* m_parent = nullptr;
    std::shared_ptr<Lifetime> m_lifetime = std::make_shared<Lifetime>();
    ImageDecodeDependencies m_decodeDependencies;
    PredecodeLoadState m_loadState;
    PredecodeActiveDecodeStore m_activeDecodes;
};
}

#endif
