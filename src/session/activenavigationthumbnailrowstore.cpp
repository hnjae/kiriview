// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnailrowstore.h"

#include "session/thumbnaillogging.h"

#include <QAbstractListModel>
#include <QDebug>
#include <utility>

namespace {
kiriview::ThumbnailSourceRevisionKey sourceKeyForRow(
    const kiriview::ActiveNavigationThumbnailRow& row, quint64 navigationGeneration)
{
    return kiriview::thumbnailSourceRevisionKey(row.number, row.url, row.label,
        kiriview::activeNavigationThumbnailPageKindIdentity(row.kind),
        kiriview::activeNavigationThumbnailSourceKindIdentity(row.sourceKind),
        navigationGeneration);
}
}

namespace kiriview {
ActiveNavigationThumbnailRowStore::ActiveNavigationThumbnailRowStore(
    QObject* owner, std::shared_ptr<ThumbnailImageStore> imageStore)
    : m_model(std::make_unique<ActiveNavigationThumbnailModel>(owner))
    , m_imageStore(std::move(imageStore))
{
    if (m_imageStore == nullptr) {
        m_imageStore = sharedThumbnailImageStore();
    }
}

ActiveNavigationThumbnailRowStore::~ActiveNavigationThumbnailRowStore() { releaseAllImages(); }

QAbstractListModel* ActiveNavigationThumbnailRowStore::model() const { return m_model.get(); }

quint64 ActiveNavigationThumbnailRowStore::navigationGeneration() const
{
    return m_navigationGeneration;
}

std::size_t ActiveNavigationThumbnailRowStore::rowCount() const { return m_rows.size(); }

std::vector<ThumbnailSourceRevisionKey> ActiveNavigationThumbnailRowStore::sourceKeys() const
{
    std::vector<ThumbnailSourceRevisionKey> result;
    result.reserve(m_rows.size());
    for (const RowState& state : m_rows) {
        result.push_back(state.sourceKey);
    }
    return result;
}

bool ActiveNavigationThumbnailRowStore::hasSameRowIdentities(
    const std::vector<ActiveNavigationThumbnailRow>& rows) const
{
    if (m_rows.size() != rows.size()) {
        return false;
    }

    for (std::size_t row = 0; row < rows.size(); ++row) {
        if (!sameRowIdentity(m_rows.at(row).row, rows.at(row))) {
            return false;
        }
    }
    return true;
}

void ActiveNavigationThumbnailRowStore::setRows(std::vector<ActiveNavigationThumbnailRow> rows)
{
    if (hasSameRowIdentities(rows)) {
        for (std::size_t row = 0; row < rows.size(); ++row) {
            m_rows.at(row).row.current = rows.at(row).current;
        }
        publishRows();
        return;
    }

    releaseAllImages();
    ++m_navigationGeneration;
    m_rowIndexByDemandIdentity.clear();
    m_rowIndexBySourceKey.clear();
    qCDebug(kiriviewThumbnailLog) << "Reset active navigation thumbnail rows generation"
                                  << m_navigationGeneration << "rowCount" << rows.size();
    m_rows.clear();
    m_rows.reserve(rows.size());
    for (ActiveNavigationThumbnailRow& row : rows) {
        RowState state;
        state.row = std::move(row);
        state.sourceKey = sourceKeyForRow(state.row, m_navigationGeneration);
        m_rows.push_back(std::move(state));
    }
    rebuildRowIndexes();
    publishRows();
}

void ActiveNavigationThumbnailRowStore::setCurrentNumber(int currentNumber)
{
    bool changed = false;
    for (RowState& state : m_rows) {
        const bool current = state.row.number == currentNumber;
        if (state.row.current == current) {
            continue;
        }
        state.row.current = current;
        changed = true;
    }

    if (changed) {
        publishRows();
    }
}

ActiveNavigationThumbnailResult ActiveNavigationThumbnailRowStore::resultAt(std::size_t row) const
{
    return m_rows.at(row).result;
}

std::optional<std::size_t> ActiveNavigationThumbnailRowStore::rowIndexForIdentity(
    int number, const QUrl& url, quint64 navigationGeneration) const
{
    const ThumbnailDemandKey identity = thumbnailDemandKey(number, url, navigationGeneration);
    if (!isValidThumbnailDemandKey(identity)) {
        return {};
    }

    const auto row = m_rowIndexByDemandIdentity.constFind(identity);
    if (row == m_rowIndexByDemandIdentity.cend()) {
        return {};
    }
    return *row;
}

std::optional<std::size_t> ActiveNavigationThumbnailRowStore::rowIndexForSourceKey(
    const ThumbnailSourceRevisionKey& sourceKey) const
{
    if (!isValidThumbnailSourceRevisionKey(sourceKey)) {
        return {};
    }

    const auto row = m_rowIndexBySourceKey.constFind(sourceKey);
    if (row == m_rowIndexBySourceKey.cend()) {
        return {};
    }
    return *row;
}

ThumbnailSourceRevisionKey ActiveNavigationThumbnailRowStore::sourceKeyAt(std::size_t row) const
{
    return m_rows.at(row).sourceKey;
}

bool ActiveNavigationThumbnailRowStore::hasUsableReadyImage(
    const ThumbnailSourceRevisionKey& sourceKey) const
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    return row.has_value() && hasUsableReadyImage(m_rows.at(*row));
}

void ActiveNavigationThumbnailRowStore::applyPending(const ThumbnailSourceRevisionKey& sourceKey)
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    if (!row.has_value() || hasUsableReadyImage(m_rows.at(*row))) {
        return;
    }

    RowState& state = m_rows.at(*row);
    releaseImage(state);
    state.result = { ActiveNavigationThumbnailResultStatus::Pending, {} };
    publishResultAt(*row);
}

void ActiveNavigationThumbnailRowStore::applyUnsupported(
    const ThumbnailSourceRevisionKey& sourceKey)
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    if (!row.has_value()) {
        return;
    }

    RowState& state = m_rows.at(*row);
    releaseImage(state);
    state.result = { ActiveNavigationThumbnailResultStatus::Unsupported, {} };
    publishResultAt(*row);
}

void ActiveNavigationThumbnailRowStore::applyFailed(const ThumbnailSourceRevisionKey& sourceKey)
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    if (!row.has_value()) {
        return;
    }

    RowState& state = m_rows.at(*row);
    if (hasUsableReadyImage(state)) {
        state.result = { ActiveNavigationThumbnailResultStatus::Ready,
            thumbnailImageSourceForId(state.imageStoreId) };
    } else {
        releaseImage(state);
        state.result = { ActiveNavigationThumbnailResultStatus::Failed, {} };
    }
    publishResultAt(*row);
}

void ActiveNavigationThumbnailRowStore::applyResult(
    const ThumbnailSourceRevisionKey& sourceKey, const ActiveNavigationThumbnailResult& result)
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    if (!row.has_value()) {
        return;
    }

    RowState& state = m_rows.at(*row);
    if (result.status != ActiveNavigationThumbnailResultStatus::Ready
        && hasUsableReadyImage(state)) {
        state.result = { ActiveNavigationThumbnailResultStatus::Ready,
            thumbnailImageSourceForId(state.imageStoreId) };
    } else {
        releaseImage(state);
        state.result = result;
    }
    publishResultAt(*row);
}

bool ActiveNavigationThumbnailRowStore::installReadyImage(
    const ThumbnailSourceRevisionKey& sourceKey, const QImage& image,
    ThumbnailImageRetentionPriority priority, bool preserveExistingReadyImage)
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    if (!row.has_value()) {
        return false;
    }

    RowState& state = m_rows.at(*row);
    if (preserveExistingReadyImage && hasUsableReadyImage(state)) {
        return true;
    }

    const QString imageId
        = m_imageStore == nullptr ? QString() : m_imageStore->insert(image, priority);
    if (imageId.isEmpty()) {
        return false;
    }

    releaseImage(state);
    state.imageStoreId = imageId;
    state.result
        = { ActiveNavigationThumbnailResultStatus::Ready, thumbnailImageSourceForId(imageId) };
    publishResultAt(*row);
    return true;
}

void ActiveNavigationThumbnailRowStore::updateRetentionPriority(
    const ThumbnailSourceRevisionKey& sourceKey, ThumbnailImageRetentionPriority priority)
{
    const std::optional<std::size_t> row = rowIndexForSourceKey(sourceKey);
    if (!row.has_value()) {
        return;
    }

    const RowState& state = m_rows.at(*row);
    if (m_imageStore != nullptr && !state.imageStoreId.isEmpty()) {
        m_imageStore->updatePriority(state.imageStoreId, priority);
    }
}

bool ActiveNavigationThumbnailRowStore::sameRowIdentity(
    const ActiveNavigationThumbnailRow& left, const ActiveNavigationThumbnailRow& right)
{
    return left.number == right.number && left.url == right.url && left.label == right.label
        && left.kind == right.kind && left.sourceKind == right.sourceKind;
}

bool ActiveNavigationThumbnailRowStore::hasUsableReadyImage(const RowState& state) const
{
    return state.result.status == ActiveNavigationThumbnailResultStatus::Ready
        && !state.imageStoreId.isEmpty() && m_imageStore != nullptr
        && !m_imageStore->image(state.imageStoreId).isNull();
}

void ActiveNavigationThumbnailRowStore::releaseImage(RowState& state)
{
    if (m_imageStore != nullptr && !state.imageStoreId.isEmpty()) {
        m_imageStore->release(state.imageStoreId);
    }
    state.imageStoreId.clear();
}

void ActiveNavigationThumbnailRowStore::releaseAllImages()
{
    for (RowState& state : m_rows) {
        releaseImage(state);
    }
}

void ActiveNavigationThumbnailRowStore::rebuildRowIndexes()
{
    m_rowIndexByDemandIdentity.clear();
    m_rowIndexBySourceKey.clear();
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        const RowState& state = m_rows.at(row);
        const ThumbnailDemandKey demandIdentity
            = thumbnailDemandKey(state.row.number, state.row.url, m_navigationGeneration);
        if (isValidThumbnailDemandKey(demandIdentity)) {
            m_rowIndexByDemandIdentity.insert(demandIdentity, row);
        }

        if (isValidThumbnailSourceRevisionKey(state.sourceKey)) {
            m_rowIndexBySourceKey.insert(state.sourceKey, row);
        }
    }
}

void ActiveNavigationThumbnailRowStore::publishRows()
{
    std::vector<ActiveNavigationThumbnailRow> rows;
    rows.reserve(m_rows.size());
    for (const RowState& state : m_rows) {
        rows.push_back(state.row);
    }
    m_model->setRows(std::move(rows), m_navigationGeneration);
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        publishResultAt(row);
    }
}

void ActiveNavigationThumbnailRowStore::publishResultAt(std::size_t row)
{
    const RowState& state = m_rows.at(row);
    m_model->setThumbnailResultAt(
        static_cast<int>(row), state.result.status, state.result.imageSource);
}
}
