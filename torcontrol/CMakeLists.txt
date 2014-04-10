if("${RT_LIBRARIES}" STREQUAL "")
    find_package(RT REQUIRED)
endif()
if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
if("${EVENT2_LIBRARIES}" STREQUAL "")
    find_package(EVENT2 REQUIRED)
endif()
include_directories(${RT_INCLUDES} ${GLIB_INCLUDES} ${EVENT2_INCLUDES})

set(torcontrol-modules
	torcontrol-circuitbuild.c
    torcontrol-logger.c
    torcontrol-pinger.c
)

## executable that can run outside of shadow
add_executable(shadow-torcontrol torcontrol-main.c torcontrol.c  ${torcontrol-modules})
target_link_libraries(shadow-torcontrol ${RT_LIBRARIES} ${GLIB_LIBRARIES} ${EVENT2_LIBRARIES})
install(TARGETS shadow-torcontrol DESTINATION bin)

## a stand-alone shared library that can plug into shadow
add_bitcode(shadow-plugin-torcontrol-bitcode 
    torcontrol-plugin.c torcontrol-main.c torcontrol.c ${torcontrol-modules})
add_plugin(shadow-plugin-torcontrol shadow-plugin-torcontrol-bitcode)
target_link_libraries(shadow-plugin-torcontrol ${RT_LIBRARIES} ${GLIB_LIBRARIES} ${EVENT2_LIBRARIES})
install(TARGETS shadow-plugin-torcontrol DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
