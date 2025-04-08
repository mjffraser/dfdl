cmake_minimum_required(VERSION 3.28)
project(distributed-file-downloader)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#externally installed libs
find_package(OpenSSL REQUIRED)

#header-only libs
set(LIB_SRC
    lib/sqlite/sqlite3.c
)

#all server src files
set(SERVER_SRC
    src/server/internal/database/db.cpp
    src/server/internal/database/internal/queries.cpp
    src/server/internal/syncing.cpp
    src/server/server.cpp
)

#all networking & API src files
set(NETWORKING_SRC
    src/networking/fileParsing.cpp
    src/networking/messageFormatting.cpp
    src/networking/internal/messageFormatting/byteOrdering.cpp
    src/networking/internal/fileParsing/fileUtil.cpp

    src/networking/socket.cpp
    src/networking/internal/sockets/socketUtil.cpp
)

set(CLIENT_SRC
    src/client/client.cpp

    #client internals
    src/client/internal/requests.cpp
    src/client/internal/clientThreads.cpp
    src/client/internal/clientConfigs.cpp

    #nested threads and util
    src/client/internal/internal/attemptServerRequest.cpp
    src/client/internal/internal/attemptPeerRequest.cpp
    src/client/internal/internal/seedThread.cpp

    #lowest level util
    src/client/internal/internal/internal/clientNetworking.cpp
    # src/client/internal/internal/internal/downloadThread.cpp
    
)

set(ALL_SRC
    ${LIB_SRC}
    ${SERVER_SRC}
    ${NETWORKING_SRC}
    ${CLIENT_SRC}
    src/main.cpp
)

add_executable(dfdl ${ALL_SRC})
target_include_directories(dfdl PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/include
                           ${CMAKE_CURRENT_SOURCE_DIR}/lib/sqlite
                          )
target_link_libraries(dfdl PRIVATE OpenSSL::SSL)
