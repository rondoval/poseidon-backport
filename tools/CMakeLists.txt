# Poseidon shellapps — per-gadget CLI tools (USB radio, cameras, UPS, rocket launcher)

set(SHELLAPPS DRadioTool PencamTool PowManTool RocketTool SonixcamTool UPSTool)

foreach(t ${SHELLAPPS})
    add_executable(${t} ${t}.c debug.c)
    add_dependencies(${t} poseidon_sfd_headers)
    target_compile_options(${t} PRIVATE
        -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
        -DPOSEIDON_BASE_NAME=ps
        -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)
    target_include_directories(${t} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR})        # local <Tool>.h, debug.h
    target_link_libraries(${t} PRIVATE poseidon_headers -lm)  # -lm: pow/atof (camera tools)
endforeach()
