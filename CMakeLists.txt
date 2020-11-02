cmake_minimum_required(VERSION 3.14)
project(GameboyVramViewer LANGUAGES CXX)

set(EXE vram_view)

if (UNIX)
	option(FETCH_LIBS "Download dependencies automatically" OFF)
else()
	option(FETCH_LIBS "Download dependencies automatically" ON)
endif()

if (FETCH_LIBS)
	include(cmake/depends.cmake)
endif()

add_executable(${EXE}
	main.cpp
)

target_include_directories(${EXE}
	PRIVATE
		${FETCHCONTENT_BASE_DIR}/sdl2-src/include
)

target_link_libraries(${EXE} SDL2)
