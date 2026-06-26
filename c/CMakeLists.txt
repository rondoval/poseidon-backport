set(SHELLCMDS
    PsdStackLoader
    AddUSBHardware
    AddUSBClasses
    PsdDevLister
    PsdErrorlog)

foreach(t ${SHELLCMDS})
    add_executable(${t} ${t}.c)
    add_dependencies(${t} poseidon_sfd_headers)
    target_compile_options(${t} PRIVATE
        -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
        -DPOSEIDON_BASE_NAME=ps        # client base-var = the program's own `ps`
        -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)
    target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${t} PRIVATE poseidon_headers)   # default CRT link
endforeach()
