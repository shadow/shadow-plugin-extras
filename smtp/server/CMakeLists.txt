project(smtpsever CXX)

find_package(EVENT2 REQUIRED)

include_directories(AFTER ${EVENT2_INCLUDES} ${CMAKE_SOURCE_DIR}/utility)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
add_definitions(-DENABLE_MY_LOG_MACROS)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -UDEBUG_PIPELINE")

set(smtpserver_sources
    smtpserver.cc
    handler.cc
    ../utility/myevent.cc
    ../utility/common.cc
)

## service library to allow smtpserver to be used by any plugin
add_library(shadow-service-smtpserver STATIC ${smtpserver_sources})
add_dependencies(shadow-service-smtpserver shadow-util)
target_link_libraries(shadow-service-smtpserver ${RT_LIBRARIES} stdc++ ${EVENT2_LIBRARIES})

# ## executable that can run outside of shadow
add_executable(shadow-smtpserver shd-smtpserver-main.cc)
target_link_libraries(shadow-smtpserver shadow-service-smtpserver ${RT_LIBRARIES} stdc++ ${EVENT2_LIBRARIES} glib-2.0)
install(TARGETS shadow-smtpserver DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
#add_bitcode(shadow-service-smtpserver-bitcode ${smtpserver_sources})
#add_bitcode(shadow-plugin-smtpserver-bitcode shd-smtpserver-plugin.cc)

## create and install a shared library that can plug into shadow
#add_plugin(shadow-plugin-smtpserver shadow-plugin-smtpserver-bitcode shadow-service-smtpserver-bitcode)
#target_link_libraries(shadow-plugin-smtpserver stdc++ ${EVENT2_LIBRARIES})
#install(TARGETS shadow-plugin-smtpserver DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
#if(SHADOW_EXPORT STREQUAL ON)
#    install(TARGETS shadow-service-smtpserver shadow-service-smtpserver-bitcode EXPORT shadow-externals DESTINATION lib)
#    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
#endif(SHADOW_EXPORT STREQUAL ON)
