// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEACTIVEDECODESTORE_H
#define KIRIVIEW_PREDECODEACTIVEDECODESTORE_H

#include "decoding/imagedecoderequest.h"
#include "predecodeactiveloads.h"

#include <QPointer>
#include <cstddef>
#include <optional>
#include <vector>

namespace kiriview {
class ImageDecodeJob;

enum class PredecodeActiveDecodeState : quint8 {
    Publishing,
    Retiring,
};

struct PredecodeRetiringDecode
{
    ImageDecodeRequest request;
    PredecodeWorkKey workKey;
};

class PredecodeActiveDecodeStore final
{
public:
    PredecodeActiveDecodeStore() = default;
    ~PredecodeActiveDecodeStore();
    Q_DISABLE_COPY_MOVE(PredecodeActiveDecodeStore)

    bool add(ImageDecodeRequest request, PredecodeWorkKey workKey, ImageDecodeJob* decodeJob);
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] PredecodeActiveLoads activeLoads() const;
    std::optional<PredecodeRetiringDecode> beginRetirement(const ImageDecodeRequest& request);
    bool retire(const ImageDecodeRequest& request);
    void cancelLocation(const DisplayedImageLocation& location);
    void cancel();

private:
    struct Entry
    {
        ImageDecodeRequest request;
        PredecodeWorkKey workKey;
        QPointer<ImageDecodeJob> decodeJob;
        PredecodeActiveDecodeState state = PredecodeActiveDecodeState::Publishing;
    };

    void shutdown();
    std::vector<Entry> m_entries;
};
}

#endif
