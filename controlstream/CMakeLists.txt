add_executable (joystick-canbridge rover-stick-ctl.c)

if (WITH_SDLPLAYER)
	add_executable (joystick-input joystick-input.c)
	target_link_libraries (joystick-input SDL2)
endif()

# test programs
add_executable (read-max31865 read-max31865.c)
