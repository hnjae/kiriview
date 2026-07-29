// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodeactivedecodestore.h"

#include "decoding/imagedecodejob.h"
#include "location/imageurl.h"
#include <algorithm>
#include <utility>

namespace {
kiriview::PredecodeWorkKey workKeyForRequest(const kiriview::ImageDecodeRequest& request)
{
    return {
        kiriview::PredecodeImageKey { request.location(), request.sourceRevision() },
        request.id(),
    };
}
}

namespace kiriview {
PredecodeActiveDecodeStore::~PredecodeActiveDecodeStore() { cancel(); }

bool PredecodeActiveDecodeStore::add(ImageDecodeRequest request, ImageDecodeJob* decodeJob)
{
    if (!normalizedValidImageUrl(request.imageUrl()).has_value() || decodeJob == nullptr
        || activeLoads().contains(workKeyForRequest(request))) {
        return false;
    }

    m_entries.push_back(Entry { std::move(request), decodeJob });
    return true;
}

std::size_t PredecodeActiveDecodeStore::size() const { return m_entries.size(); }

PredecodeActiveLoads PredecodeActiveDecodeStore::activeLoads() const
{
    std::vector<PredecodeWorkKey> keys;
    keys.reserve(m_entries.size());
    for (const Entry& entry : m_entries) {
        keys.push_back(workKeyForRequest(entry.request));
    }
    return PredecodeActiveLoads::fromWorkKeys(keys);
}

std::optional<ImageDecodeRequest> PredecodeActiveDecodeStore::finish(
    const ImageDecodeRequest& request)
{
    const auto entry = std::ranges::find_if(m_entries,
        [&request](const Entry& candidate) { return candidate.request.matches(request); });
    if (entry == m_entries.end()) {
        return std::nullopt;
    }

    ImageDecodeRequest finishedRequest = entry->request;
    if (entry->decodeJob != nullptr) {
        entry->decodeJob->deleteLater();
    }
    m_entries.erase(entry);
    return finishedRequest;
}

void PredecodeActiveDecodeStore::cancel()
{
    for (const Entry& entry : m_entries) {
        if (entry.decodeJob == nullptr) {
            continue;
        }

        entry.decodeJob->cancel();
        entry.decodeJob->deleteLater();
    }
    m_entries.clear();
}
}
