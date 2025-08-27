# Module integrity check is seen as OOB access by sanitizers, and sanitizer instrumentation
# breaks integrity check assumptions. Only enable FIPS module when not running with sanitizers
add_subdirectory(posix)

if(WIN32)
    add_subdirectory(windows)
endif()
