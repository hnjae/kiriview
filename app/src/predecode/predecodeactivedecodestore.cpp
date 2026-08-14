// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodeactivedecodestore.h"

#include "decoding/imagedecodejob.h"
#include "location/imageurl.h"
#include <algorithm>
#include <utility>

namespace kiriview {
PredecodeActiveDecodeStore::~PredecodeActiveDecodeStore() { shutdown(); }

bool PredecodeActiveDecodeStore::add(
    ImageDecodeRequest request, PredecodeWorkKey workKey, ImageDecodeJob* decodeJob)
{
    if (!normalizedValidImageUrl(request.imageUrl()).has_value() || decodeJob == nullptr
        || request.location() != workKey.image.location
        || (!workKey.image.sourceRevision.isValid() && !workKey.scope.isValid())
        || std::ranges::any_of(
            m_entries, [&request](const Entry& entry) { return entry.request.matches(request); })
        || activeLoads().contains(workKey)) {
        return false;
    }

    m_entries.push_back(Entry { std::move(request), std::move(workKey), decodeJob,
        PredecodeActiveDecodeState::Publishing });
    return true;
}

std::size_t PredecodeActiveDecodeStore::size() const { return m_entries.size(); }

PredecodeActiveLoads PredecodeActiveDecodeStore::activeLoads() const
{
    std::vector<PredecodeActiveLoads::Entry> entries;
    entries.reserve(m_entries.size());
    for (const Entry& entry : m_entries) {
        entries.push_back(PredecodeActiveLoads::Entry {
            entry.workKey,
            entry.state == PredecodeActiveDecodeState::Retiring,
        });
    }
    return PredecodeActiveLoads::fromEntries(entries);
}

std::optional<PredecodeRetiringDecode> PredecodeActiveDecodeStore::beginRetirement(
    const ImageDecodeRequest& request)
{
    const auto entry = std::ranges::find_if(m_entries,
        [&request](const Entry& candidate) { return candidate.request.matches(request); });
    if (entry == m_entries.end() || entry->state != PredecodeActiveDecodeState::Publishing) {
        return std::nullopt;
    }

    entry->state = PredecodeActiveDecodeState::Retiring;
    return PredecodeRetiringDecode { entry->request, entry->workKey };
}

bool PredecodeActiveDecodeStore::retire(const ImageDecodeRequest& request)
{
    const auto entry = std::ranges::find_if(m_entries,
        [&request](const Entry& candidate) { return candidate.request.matches(request); });
    if (entry == m_entries.end() || entry->state != PredecodeActiveDecodeState::Retiring) {
        return false;
    }

    if (entry->decodeJob != nullptr) {
        entry->decodeJob->deleteLater();
    }
    m_entries.erase(entry);
    return true;
}

void PredecodeActiveDecodeStore::cancelLocation(const DisplayedImageLocation& location)
{
    for (Entry& entry : m_entries) {
        if (entry.request.location() != location) {
            continue;
        }
        if (entry.state == PredecodeActiveDecodeState::Retiring) {
            continue;
        }
        entry.state = PredecodeActiveDecodeState::Retiring;
        if (entry.decodeJob != nullptr) {
            entry.decodeJob->cancel();
        }
    }
}

void PredecodeActiveDecodeStore::cancel()
{
    for (Entry& entry : m_entries) {
        if (entry.state == PredecodeActiveDecodeState::Retiring) {
            continue;
        }
        entry.state = PredecodeActiveDecodeState::Retiring;
        if (entry.decodeJob != nullptr) {
            entry.decodeJob->cancel();
        }
    }
}

void PredecodeActiveDecodeStore::shutdown()
{
    cancel();
    for (const Entry& entry : m_entries) {
        if (entry.decodeJob != nullptr) {
            entry.decodeJob->deleteLater();
        }
    }
    m_entries.clear();
}
}
