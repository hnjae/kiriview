// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

void TestKiriDocumentSessionNavigation::emptySessionProjectsUnavailableActiveNavigation()
{
    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    std::unique_ptr<KiriDocumentSession> session = createSession(directMediaNavigationProvider);

    compareUnavailableActiveNavigation(*session);
}
