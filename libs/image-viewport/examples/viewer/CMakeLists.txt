# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

qt_add_executable(imageviewport_viewer main.cpp)

qt_add_qml_module(
    imageviewport_viewer
    URI
    ImageViewportViewer
    VERSION
    1.0
    QML_FILES
    Main.qml)

target_link_libraries(imageviewport_viewer PRIVATE ImageViewport Qt6::Quick)
