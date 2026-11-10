cmake_minimum_required(VERSION 3.16)
project(UppBuilder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    UppBuilder.cpp
    Bridge.cpp
    UppParser.cpp
    DepGraph.cpp
    HeaderScanner.cpp
    UmkDriver.cpp
    RuntimeDeps.cpp
    FlagAuditor.cpp
    UseModel.cpp
    PolicyEngine.cpp
    EscBuildParser.cpp
    ProfileManager.cpp
    ProviderPlanner.cpp
    OverlayManager.cpp
    EmergeEngine.cpp
    main.cpp
)

set(HEADERS
    UppBuilder.h
    Bridge.h
    UppParser.h
    DepGraph.h
    HeaderScanner.h
    UmkDriver.h
    RuntimeDeps.h
    FlagAuditor.h
    UseModel.h
    PolicyEngine.h
    EscBuildParser.h
    ProfileManager.h
    ProviderPlanner.h
    OverlayManager.h
    EmergeEngine.h
)

add_executable(uppbuilder ${SOURCES} ${HEADERS})

find_package(Threads REQUIRED)
target_link_libraries(uppbuilder PRIVATE Threads::Threads)
