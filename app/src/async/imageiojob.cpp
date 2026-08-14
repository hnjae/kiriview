// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/imageiojob.h"

#include "async/imagecallback.h"

#include <QObject>
#include <utility>

namespace kiriview {
ImageIoJobState::ImageIoJobState(QObject* object, CancelCallback cancelCallback,
    ImageIoJobCancellationRetirement cancellationRetirement)
    : m_token(object)
    , m_activeObject(object)
    , m_cancelCallback(std::move(cancelCallback))
    , m_cancellationRetirement(cancellationRetirement)
{
}

bool ImageIoJobState::claim(QObject* object)
{
    if (m_activeObject.isNull() || object != m_activeObject.data()) {
        return false;
    }

    m_activeObject.clear();
    m_cancelCallback = {};
    if (m_cancellationRetirement == ImageIoJobCancellationRetirement::Synchronous) {
        retire();
    }
    return true;
}

void ImageIoJobState::cancel()
{
    if (m_activeObject.isNull()) {
        m_cancelCallback = {};
        if (m_cancellationRetirement == ImageIoJobCancellationRetirement::Synchronous) {
            retire();
        }
        return;
    }

    QObject* object = m_activeObject.data();
    CancelCallback cancelCallback = std::move(m_cancelCallback);
    m_activeObject.clear();
    invokeIfSet(cancelCallback, object);
    if (m_cancellationRetirement == ImageIoJobCancellationRetirement::Synchronous) {
        retire();
    }
}

void ImageIoJobState::retire()
{
    RetirementCallback callback;
    {
        const std::scoped_lock lock(m_retirementMutex);
        if (m_retired) {
            return;
        }
        m_retired = true;
        callback = std::move(m_retirementCallback);
    }
    invokeIfSet(callback);
}

void ImageIoJobState::setRetirementCallback(RetirementCallback callback)
{
    bool invokeNow = false;
    {
        const std::scoped_lock lock(m_retirementMutex);
        if (m_retired) {
            invokeNow = true;
        } else {
            m_retirementCallback = std::move(callback);
        }
    }
    if (invokeNow) {
        invokeIfSet(callback);
    }
}

bool ImageIoJobState::isActive() const { return !m_activeObject.isNull(); }

QObject* ImageIoJobState::token() const { return m_token.data(); }

ImageIoJobCompletion::ImageIoJobCompletion(std::shared_ptr<ImageIoJobState> state)
    : m_state(std::move(state))
{
}

QObject* ImageIoJobCompletion::object() const
{
    return m_state == nullptr ? nullptr : m_state->token();
}

bool ImageIoJobCompletion::isActive() const { return m_state != nullptr && m_state->isActive(); }

void ImageIoJobCompletion::cancel() const
{
    if (m_state == nullptr) {
        return;
    }

    m_state->cancel();
}

void ImageIoJobCompletion::retire() const
{
    if (m_state != nullptr) {
        m_state->retire();
    }
}

ImageIoJob::ImageIoJob(QObject* object, CancelCallback cancelCallback,
    ImageIoJobCancellationRetirement cancellationRetirement)
    : m_state(std::make_shared<ImageIoJobState>(
          object, std::move(cancelCallback), cancellationRetirement))
{
}

ImageIoJob::~ImageIoJob() { cancel(); }

ImageIoJob& ImageIoJob::operator=(ImageIoJob&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    cancel();
    m_state = std::move(other.m_state);
    return *this;
}

void ImageIoJob::cancel()
{
    if (m_state == nullptr) {
        return;
    }

    m_state->cancel();
}

void ImageIoJob::setRetirementCallback(RetirementCallback callback)
{
    if (m_state == nullptr) {
        invokeIfSet(callback);
        return;
    }
    m_state->setRetirementCallback(std::move(callback));
}

bool ImageIoJob::isActive() const { return m_state != nullptr && m_state->isActive(); }

ImageIoJobCompletion ImageIoJob::completion() const { return ImageIoJobCompletion(m_state); }
}
