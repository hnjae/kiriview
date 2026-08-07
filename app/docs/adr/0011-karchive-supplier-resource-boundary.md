# KArchive Supplier Resource Boundary

## Context

Opened-collection media entry sources use KArchive for ZIP, TAR, and 7z and libarchive for RAR. KArchive exposes archive entries only after its format supplier has parsed the archive and created the representation needed by the opened-archive interface. Directory enumeration then exposes a materialized entry-name list. Application-owned collection-enumeration admission can reject the complete result but cannot prevent the supplier work and representations that precede that boundary.

With the current supplier interfaces, the considered repository-local hard-containment design would require an independently resource-contained supplier process. Replacing KArchive with libarchive would change supported backend capabilities and would not by itself establish a bounded pre-admission path for every format. Either choice adds substantial complexity or capability risk relative to retaining the current supplier boundary.

## Decision

KArchive remains the opened-collection media entry supplier for ZIP, TAR, and 7z, and libarchive remains the supplier for RAR. KiriView accepts the memory and processing risk of KArchive-internal parsing and of supplier representations materialized before individual entries become available for application admission.

The collection-access owner applies its aggregate enumeration policy at the earliest boundary where the supplier exposes entries for application-controlled traversal. A successful supplier open does not accept or publish a candidate snapshot: KiriView accepts only a complete candidate snapshot within the application limits and discards the whole enumeration on a resource-limit, cancellation, or stale-lifecycle outcome.

Application collection-enumeration limits do not guarantee bounds on supplier-internal memory or processing time, or recovery from supplier allocation failure. KiriView does not add a containment process, a second parser preflight, or a backend migration for this risk.

## Consequences

Existing ZIP, TAR, 7z, and RAR collection capabilities and their scope-owned media-entry-source lifetimes remain unchanged. After a supplier exposes its representation, application admission still bounds application-controlled traversal and retained candidate identity data and prevents partial publication.

A hostile or unusually large KArchive-backed archive may consume excessive memory or processing time before KiriView can return a typed application resource-limit outcome. If supplier work exhausts process resources, the application cannot guarantee recovery or a typed failure.

This decision should be reconsidered if the supplier provides a bounded admission interface, operational evidence makes the accepted risk disproportionate, or collection capability requirements justify changing the backend boundary.
