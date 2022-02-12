add_executable (videostream videostream.cc mjpegcam.cc)
target_link_libraries (videostream LINK_PUBLIC uvgrtp pthread)
install(TARGETS videostream DESTINATION bin)

# This is the client side video receiver
if (WITH_SDLPLAYER)
	add_executable (recvjpegsdl recvjpegsdl.cc mjpegcam.cc)
	target_link_libraries (recvjpegsdl LINK_PUBLIC uvgrtp pthread jpeg SDL2 SDL2_image)
endif()

