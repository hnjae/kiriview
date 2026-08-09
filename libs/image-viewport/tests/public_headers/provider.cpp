// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imagesequenceprovider.h>

#include <QtCore/QString>

#include <type_traits>
#include <utility>

template <typename Token, typename = void> struct ProviderFailureAcceptsDiagnostic : std::false_type
{
};

template <typename Token>
struct ProviderFailureAcceptsDiagnostic<Token,
    std::void_t<decltype(ImageSequenceProviderEvent::failed(
        std::declval<Token>(), std::declval<QString>()))>> : std::true_type
{
};

template <typename Token, typename = void>
struct ProviderCancellationAcceptsDiagnostic : std::false_type
{
};

template <typename Token>
struct ProviderCancellationAcceptsDiagnostic<Token,
    std::void_t<decltype(ImageSequenceProviderEvent::cancelled(
        std::declval<Token>(), std::declval<QString>()))>> : std::true_type
{
};

template <typename Token, typename = void>
struct ProviderUnsupportedAcceptsDiagnostic : std::false_type
{
};

template <typename Token>
struct ProviderUnsupportedAcceptsDiagnostic<Token,
    std::void_t<decltype(ImageSequenceProviderEvent::unsupported(std::declval<Token>(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest, std::declval<QString>()))>>
    : std::true_type
{
};

static_assert(std::is_base_of_v<QObject, ImageSequenceProviderSession>);
static_assert(std::is_enum_v<ImageSequenceProviderRequestKind>);
static_assert(std::is_enum_v<ImageSequenceProviderEventSubmissionOutcome>);
static_assert(std::is_same_v<decltype(std::declval<ImageSequenceProviderSession&>().submitEvent(
                                 std::declval<const ImageSequenceProviderEvent&>())),
    ImageSequenceProviderEventSubmissionOutcome>);
static_assert(std::is_copy_constructible_v<ImageSequenceProviderDescriptor>);
static_assert(std::is_same_v<decltype(ImageSequenceProviderDisplayDemand().allocationGeneration()),
    ImageViewportAllocationGenerationToken>);
static_assert(!ProviderFailureAcceptsDiagnostic<ImageSequenceProviderRequestToken>::value);
static_assert(!ProviderCancellationAcceptsDiagnostic<ImageSequenceProviderRequestToken>::value);
static_assert(!ProviderUnsupportedAcceptsDiagnostic<ImageSequenceProviderRequestToken>::value);
