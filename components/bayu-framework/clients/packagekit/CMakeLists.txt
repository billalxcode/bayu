bayu_add_client(packagekit TARGET PackageKitClient
    SOURCES packagekit.cpp
    HEADERS packagekit.h
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
