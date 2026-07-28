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

class PredecodeActiveDecodeStore final
{
public:
    PredecodeActiveDecodeStore() = default;
    ~PredecodeActiveDecodeStore();
    Q_DISABLE_COPY_MOVE(PredecodeActiveDecodeStore)

    bool add(ImageDecodeRequest request, ImageDecodeJob* decodeJob);
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool contains(const DisplayedImageLocation& location) const;
    [[nodiscard]] PredecodeActiveLoads activeLoads() const;
    std::optional<ImageDecodeRequest> finish(const ImageDecodeRequest& request);
    void cancel();

private:
    struct Entry
    {
        ImageDecodeRequest request;
        QPointer<ImageDecodeJob> decodeJob;
    };

    std::vector<Entry> m_entries;
};
}

#endif
