// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodeactivedecodestore.h"

#include "decoding/imagedecodejob.h"
#include "location/imageurl.h"
#include <algorithm>
#include <utility>

namespace kiriview {
PredecodeActiveDecodeStore::~PredecodeActiveDecodeStore() { cancel(); }

bool PredecodeActiveDecodeStore::add(ImageDecodeRequest request, ImageDecodeJob* decodeJob)
{
    if (!normalizedValidImageUrl(request.imageUrl()).has_value() || decodeJob == nullptr
        || contains(request.location())) {
        return false;
    }

    m_entries.push_back(Entry { std::move(request), decodeJob });
    return true;
}

std::size_t PredecodeActiveDecodeStore::size() const { return m_entries.size(); }

bool PredecodeActiveDecodeStore::contains(const DisplayedImageLocation& location) const
{
    if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
        return false;
    }

    return std::ranges::any_of(m_entries,
        [&location](const Entry& entry) { return entry.request.location() == location; });
}

PredecodeActiveLoads PredecodeActiveDecodeStore::activeLoads() const
{
    std::vector<DisplayedImageLocation> locations;
    locations.reserve(m_entries.size());
    for (const Entry& entry : m_entries) {
        locations.push_back(entry.request.location());
    }
    return PredecodeActiveLoads::fromLocations(locations);
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
