// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_SCOPEDFILEDESCRIPTOR_P_H
#define KIRIVIEW_SCOPEDFILEDESCRIPTOR_P_H

#include <unistd.h>
#include <utility>

namespace kiriview::MediaEntrySourceBackendDetail {
class ScopedFileDescriptor final
{
public:
    ScopedFileDescriptor() = default;

    explicit ScopedFileDescriptor(int fileDescriptor)
        : m_fileDescriptor(fileDescriptor)
    {
    }

    ~ScopedFileDescriptor() { reset(); }

    ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
    ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;

    ScopedFileDescriptor(ScopedFileDescriptor&& other) noexcept
        : m_fileDescriptor(other.release())
    {
    }

    ScopedFileDescriptor& operator=(ScopedFileDescriptor&& other) noexcept
    {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] int get() const { return m_fileDescriptor; }
    [[nodiscard]] explicit operator bool() const { return m_fileDescriptor >= 0; }

    [[nodiscard]] int release() { return std::exchange(m_fileDescriptor, -1); }

    void reset(int fileDescriptor = -1)
    {
        if (m_fileDescriptor >= 0) {
            ::close(m_fileDescriptor);
        }
        m_fileDescriptor = fileDescriptor;
    }

private:
    int m_fileDescriptor = -1;
};
}

#endif
