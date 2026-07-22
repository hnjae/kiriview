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

quint64 nextNavigationGeneration(quint64 generation)
{
    ++generation;
    if (generation == 0) {
        ++generation;
    }
    return generation;
}
}

namespace kiriview {
ActiveNavigationThumbnailRowStore::ActiveNavigationThumbnailRowStore(
    std::shared_ptr<ThumbnailImageStore> imageStore)
    : m_model(std::make_unique<ActiveNavigationThumbnailModel>())
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

ActiveNavigationThumbnailSchedulingSnapshot
ActiveNavigationThumbnailRowStore::schedulingSnapshot() const
{
    ActiveNavigationThumbnailSchedulingSnapshot snapshot;
    snapshot.navigationGeneration = m_navigationGeneration;
    snapshot.rows.reserve(m_rows.size());
    for (const RowState& state : m_rows) {
        snapshot.rows.push_back(state.sourceKey);
    }
    return snapshot;
}

ActiveNavigationThumbnailRowUpdatePlan ActiveNavigationThumbnailRowStore::prepareRows(
    std::vector<ActiveNavigationThumbnailRow> rows) const
{
    int currentNumber = 0;
    for (const ActiveNavigationThumbnailRow& row : rows) {
        if (row.current) {
            currentNumber = row.number;
            break;
        }
    }
    for (ActiveNavigationThumbnailRow& row : rows) {
        row.current = currentNumber != 0 && row.number == currentNumber;
    }
    ActiveNavigationThumbnailRowUpdateKind kind
        = ActiveNavigationThumbnailRowUpdateKind::ProjectionOnly;
    quint64 targetGeneration = m_navigationGeneration;
    if (m_rows.size() != rows.size()) {
        kind = ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement;
        targetGeneration = nextNavigationGeneration(m_navigationGeneration);
    }
    std::vector<ThumbnailSourceRevisionKey> sourceKeys;
    sourceKeys.reserve(rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row) {
        ThumbnailSourceRevisionKey sourceKey = sourceKeyForRow(rows.at(row), targetGeneration);
        if (kind != ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement) {
            const RowState& existing = m_rows.at(row);
            if (!sameRowIdentity(existing.sourceKey.row, sourceKey.row)) {
                kind = ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement;
                targetGeneration = nextNavigationGeneration(m_navigationGeneration);
                sourceKeys.clear();
                break;
            }
            if (existing.sourceKey.sourceUrl != sourceKey.sourceUrl) {
                kind = ActiveNavigationThumbnailRowUpdateKind::SourceRefresh;
            }
        }
        sourceKeys.push_back(std::move(sourceKey));
    }
    if (kind == ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement) {
        sourceKeys.clear();
        sourceKeys.reserve(rows.size());
        for (const ActiveNavigationThumbnailRow& row : rows) {
            sourceKeys.push_back(sourceKeyForRow(row, targetGeneration));
        }
    }
    return ActiveNavigationThumbnailRowUpdatePlan(
        kind, currentNumber, targetGeneration, std::move(rows), std::move(sourceKeys));
}

ActiveNavigationThumbnailRowCommit ActiveNavigationThumbnailRowStore::commitRows(
    ActiveNavigationThumbnailRowUpdatePlan plan)
{
    const auto kind = plan.m_kind;
    if (kind != ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement) {
        for (std::size_t row = 0; row < plan.m_rows.size(); ++row) {
            m_rows.at(row).row = std::move(plan.m_rows.at(row));
            if (kind == ActiveNavigationThumbnailRowUpdateKind::SourceRefresh) {
                m_rows.at(row).sourceKey = std::move(plan.m_sourceKeys.at(row));
            }
        }
        if (kind == ActiveNavigationThumbnailRowUpdateKind::SourceRefresh) {
            rebuildRowIndexes();
        }
        publishRows();
        return { kind, plan.m_currentNumber,
            kind == ActiveNavigationThumbnailRowUpdateKind::SourceRefresh
                ? std::optional<ActiveNavigationThumbnailSchedulingSnapshot>(schedulingSnapshot())
                : std::nullopt };
    }

    releaseAllImages();
    m_navigationGeneration = plan.m_navigationGeneration;
    m_rowIndexBySourceKey.clear();
    qCDebug(kiriviewThumbnailLog) << "Reset active navigation thumbnail rows generation"
                                  << m_navigationGeneration << "rowCount" << plan.m_rows.size();
    m_rows.clear();
    m_rows.reserve(plan.m_rows.size());
    for (std::size_t row = 0; row < plan.m_rows.size(); ++row) {
        RowState state;
        state.row = std::move(plan.m_rows.at(row));
        state.sourceKey = std::move(plan.m_sourceKeys.at(row));
        m_rows.push_back(std::move(state));
    }
    rebuildRowIndexes();
    publishRows();
    return { kind, plan.m_currentNumber, schedulingSnapshot() };
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
    const ThumbnailRowKey& left, const ThumbnailRowKey& right)
{
    return sameThumbnailRowKey(left, right);
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
    m_rowIndexBySourceKey.clear();
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        const RowState& state = m_rows.at(row);
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
