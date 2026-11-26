# Headless Cemu frontend (no wxWidgets). Built through upstream Cemu with -DCEMU_FRONTEND_DIR=<this dir>.
add_executable(cemu-headless
	main.cpp
	WindowSystemHeadless.cpp
	NullRenderer.cpp
)
cemu_use_precompiled_header(cemu-headless)
target_include_directories(cemu-headless PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cemu-headless PRIVATE
	CemuAudio
	CemuCafe
	CemuCommon
	CemuComponents
	CemuConfig
	CemuGui
	CemuInput
	CemuUtil
	CemuResource
)
if(EMSCRIPTEN)
	include(${CMAKE_CURRENT_LIST_DIR}/emscripten.cmake OPTIONAL)
endif()
