find_package (PkgConfig REQUIRED)
pkg_check_modules(FTDI1 REQUIRED libftdi1)

include_directories(${FTDI1_INCLUDE_DIRS})
link_directories(${FTDI1_LIBRARY_DIRS})

add_executable(papilio-prog
	bitfile.cpp butterfly.cpp devicedb.cpp iobase.cpp
	ioftdi.cpp jtag.cpp progalgspi.cpp progalgxc3s.cpp tools.cpp
)

add_definitions(${FTDI1_CFLAGS_OTHER})
target_link_libraries(papilio-prog ${FTDI1_LIBRARIES})

install(TARGETS papilio-prog DESTINATION bin)

install(FILES bscan_spi_lx9.bit bscan_spi_xc3s100e.bit
	bscan_spi_xc3s250e.bit bscan_spi_xc3s500e.bit
	DESTINATION etc)
