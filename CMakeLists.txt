#https://github.com/meemknight/cmakeSetup

cmake_minimum_required(VERSION 3.16)

project(rayTracer)

# Force static library builds for subprojects (most well-behaved projects honor this)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries by default" FORCE)

# Force static MSVC runtime (MultiThreaded / MultiThreadedDebug) for all configurations.
# Setting this as a CACHE variable before configuring subprojects ensures they also see it.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Use static MSVC runtime" FORCE)
endif()


# SDL3: Use shared (DLL) in Debug, static in Release
set(SDL3_STATIC $<NOT:$<CONFIG:Debug>> CACHE BOOL "" FORCE)
set(SDL3_SHARED $<CONFIG:Debug> CACHE BOOL "" FORCE)

set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/SDL-Main)			#window oppener
add_subdirectory(thirdparty/glad)				#opengl loader
add_subdirectory(thirdparty/stb_image)			#loading immaged
add_subdirectory(thirdparty/stb_truetype)		#loading ttf files
add_subdirectory(thirdparty/glm)				#math
add_subdirectory(thirdparty/imgui-docking)		#ui


# Define MY_SOURCES to be a list of all the source files for my game 
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")


add_executable("${CMAKE_PROJECT_NAME}")

set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 17)

target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/") # This is useful to get an ASSETS_PATH in your IDE during development but you should comment this if you compile a release version and uncomment the next line
#target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/") # Uncomment this line to setup the ASSETS_PATH macro to the final assets directory when you share the game


target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})


if(MSVC) # If using the VS compiler...

	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)

	#remove console
	#set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	
	# The global CMAKE_MSVC_RUNTIME_LIBRARY was set above; if you want to be explicit for this target you can set the target property:
	set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()


target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")


# Link SDL3::SDL3 (shared in Debug, static in Release)
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
    glm
    $<$<CONFIG:Debug>:SDL3::SDL3>
    $<$<NOT:$<CONFIG:Debug>>:SDL3::SDL3-static>
    glad stb_image stb_truetype imgui)