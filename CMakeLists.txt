cmake_minimum_required(VERSION 3.14)
project(poseidon_backport VERSION 0.0.0 LANGUAGES C ASM)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

# sfdc: required from Phase 1 onward (library header generation). Discovered, not required yet.
find_program(SFDC_EXECUTABLE sfdc)

# NOTE: emu68-common (shared debug backend / minlist / etc.) is DEFERRED — it will be
# added as a submodule and wired here (incl. EMU68_DEBUG_BACKEND) only if/when a
# component needs it. Not present in Phase 0.

# --- Components are added per-phase. ---
add_subdirectory(poseidon.library)     # Phase 1
# add_subdirectory(usbclass.library)   # Phase 2
# add_subdirectory(classes/hub)        # Phase 4 ...
