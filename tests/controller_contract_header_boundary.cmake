set(controller_header "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontroller_p.h")
set(contract_headers
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerassignmentcontract_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollercommandcontract_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollermetadatacontract_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerplaybackcontract_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerprovidercontract_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerrendercontract_p.h")

file(READ "${controller_header}" controller_content)

set(controller_forbidden_definitions
    "struct[ \t\r\n]+ViewportProvider[A-Za-z0-9_]*[ \t\r\n]*\\{"
    "struct[ \t\r\n]+ViewportRender[A-Za-z0-9_]*[ \t\r\n]*\\{"
    "struct[ \t\r\n]+ViewportCommandResult[ \t\r\n]*\\{"
    "struct[ \t\r\n]+ViewportPlaybackAdvanceResult[ \t\r\n]*\\{"
    "struct[ \t\r\n]+ViewportMetadataProjection[ \t\r\n]*\\{")

set(violations)
foreach(forbidden_pattern IN LISTS controller_forbidden_definitions)
    if(controller_content MATCHES "${forbidden_pattern}")
        list(APPEND violations "${controller_header}: ${forbidden_pattern}")
    endif()
endforeach()

foreach(contract_header IN LISTS contract_headers)
    file(READ "${contract_header}" contract_content)
    if(contract_content MATCHES "#[ \t]*include[ \t]*\"viewportcontroller_p\\.h\"")
        list(APPEND violations "${contract_header}: contract headers must not include the controller facade")
    endif()
    if(contract_content MATCHES "#[ \t]*include[ \t]*\"viewportcontroller[A-Za-z0-9_]*helpers_p\\.h\"")
        list(APPEND violations "${contract_header}: contract headers must not include implementation helper headers")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Controller facade DTOs must live in acyclic owner contract headers:\n  ${violation_report}"
    )
endif()
