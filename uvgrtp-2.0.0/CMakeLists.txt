cmake_minimum_required(VERSION 3.5)

project(uvgrtp)

option(DISABLE_CRYPTO "Do not build uvgRTP with crypto enabled")
option(PTHREADS_PATH  "Path to POSIX threads static library")
option(CRYPTOPP_PATH  "Path to Crypto++ static library")

add_library(uvgrtp STATIC
    src/clock.cc
    src/crypto.cc
    src/dispatch.cc
    src/frame.cc
    src/hostname.cc
    src/lib.cc
    src/media_stream.cc
    src/mingw_inet.cc
    src/multicast.cc
    src/pkt_dispatch.cc
    src/poll.cc
    src/queue.cc
    src/random.cc
    src/rtcp.cc
    src/rtp.cc
    src/runner.cc
    src/session.cc
    src/socket.cc
    src/zrtp.cc
    src/holepuncher.cc
    src/formats/media.cc
    src/formats/h26x.cc
    src/formats/h264.cc
    src/formats/h264_pkt_handler.cc
    src/formats/h265.cc
    src/formats/h265_pkt_handler.cc
    src/formats/h266.cc
    src/formats/h266_pkt_handler.cc
    src/zrtp/zrtp_receiver.cc
    src/zrtp/hello.cc
    src/zrtp/hello_ack.cc
    src/zrtp/commit.cc
    src/zrtp/dh_kxchng.cc
    src/zrtp/confirm.cc
    src/zrtp/confack.cc
    src/zrtp/error.cc
    src/rtcp/app.cc
    src/rtcp/sdes.cc
    src/rtcp/bye.cc
    src/rtcp/receiver.cc
    src/rtcp/sender.cc
    src/rtcp/rtcp_runner.cc
    src/srtp/base.cc
    src/srtp/srtp.cc
    src/srtp/srtcp.cc
)

target_include_directories(uvgrtp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/formats
        ${PROJECT_SOURCE_DIR}/include/zrtp
        ${PROJECT_SOURCE_DIR}/include/rtcp
        ${PROJECT_SOURCE_DIR}/include/srtp
)

set(LIBRARY_PATHS "")

if (PTHREADS_PATH)
    separate_arguments(LIBRARY_PATHS NATIVE_COMMAND ${PTHREADS_PATH})
endif (PTHREADS_PATH)

if (CRYPTOPP_PATH AND NOT DISABLE_CRYPTO)
    separate_arguments(LIBRARY_PATHS NATIVE_COMMAND "${LIBRARY_PATHS} ${CRYPTOPP_PATH}")
endif (CRYPTOPP_PATH AND NOT DISABLE_CRYPTO)

if (UNIX)
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wuninitialized -Wshadow -O2 -std=c++17  -DNDEBUG")

    if (DISABLE_CRYPTO)
        string(APPEND CMAKE_CXX_FLAGS " -D__RTP_NO_CRYPTO__")
    endif(DISABLE_CRYPTO)

    # if (NOT "${LIBRARY_PATHS}" STREQUAL "")
    #     add_custom_command(TARGET uvgrtp POST_BUILD
    #         COMMAND ar crsT ARGS libuvgrtp_thin.a libuvgrtp.a ${LIBRARY_PATHS}
    #         BYPRODUCTS libuvgrtp_thin.a
    #         COMMENT "Creating combined archive"
    #     )
    #     install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libuvgrtp_thin.a
    #             RENAME libuvgrtp.a
    #             DESTINATION /usr/local/lib
    #     )
    # else (NOT "${LIBRARY_PATHS}" STREQUAL "")
        install(TARGETS uvgrtp
                ARCHIVE
                DESTINATION /usr/local/lib
        )
    # endif (NOT "${LIBRARY_PATHS}" STREQUAL "")

    install(DIRECTORY include/ DESTINATION /usr/local/include/uvgrtp
            FILES_MATCHING PATTERN "*.hh"
    )
endif (UNIX)

if (WIN32)
    set(CMAKE_CXX_FLAGS "/O2 /std:c++17 /DNDEBUG")
    SET(OUT_DIR ${CMAKE_BINARY_DIR}/Debug)

    if (DISABLE_CRYPTO)
        string(APPEND CMAKE_CXX_FLAGS " /D__RTP_NO_CRYPTO__")
    endif(DISABLE_CRYPTO)

    # if (NOT "${LIBRARY_PATHS}" STREQUAL "")
    #     add_custom_command(TARGET uvgrtp POST_BUILD
    #         COMMAND lib /out:${OUT_DIR}/uvgrtp.lib ${OUT_DIR}/uvgrtp.lib ${LIBRARY_PATHS}
    #         BYPRODUCTS uvgrtp.lib
    #         COMMENT "Creating combined archive"
    #     )
    # else (NOT "${LIBRARY_PATHS}" STREQUAL "")
        install(TARGETS uvgrtp
                ARCHIVE
                DESTINATION ${PROJECT_BINARY_DIR}/lib
        )
    # endif (NOT "${LIBRARY_PATHS}" STREQUAL "")

    install(DIRECTORY include/ DESTINATION ${PROJECT_BINARY_DIR}/include
            FILES_MATCHING PATTERN "*.hh"
    )
endif (WIN32)
