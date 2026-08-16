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
        -Os                            # cold shell commands: size over speed
        -DPOSEIDON_BASE_NAME=ps)       # client base-var = the program's own `ps`
    target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${t} PRIVATE poseidon_headers)   # default CRT link
endforeach()
