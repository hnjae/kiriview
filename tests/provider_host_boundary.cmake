set(private_header "${IMAGEVIEWPORT_SOURCE_DIR}/src/imageviewport_p.h")

file(READ "${private_header}" private_content)

set(violations)
if(private_content MATCHES "class[ \t\r\n]+ImageViewportPrivate[^{;]*ViewportProviderBridgeClient")
    list(APPEND violations
        "${private_header}: ImageViewportPrivate must not implement ViewportProviderBridgeClient")
endif()
if(private_content MATCHES "ViewportProviderBridge[ \t\r\n]+(providerBridge|secondaryProviderBridge)")
    list(APPEND violations "${private_header}: provider bridge storage must live behind the provider host")
endif()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Provider bridge client behavior must be isolated behind the item-side provider host:\n  ${violation_report}"
    )
endif()
