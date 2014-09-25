# - Check for the presence of libspdylay
#
# The following variables are set when SPDYLAY is found:
#  HAVE_SPDYLAY       = Set to true, if all components of SPDYLAY
#                          have been found.
#  SPDYLAY_INCLUDES   = Include path for the header files of SPDYLAY
#  SPDYLAY_LIBRARIES  = Link these to use SPDYLAY

## -----------------------------------------------------------------------------
## Check for the header files

find_path (SPDYLAY_INCLUDES spdylay/spdylay.h
  PATHS ${CMAKE_EXTRA_INCLUDES} PATH_SUFFIXES spdylay/ spdylay/include NO_DEFAULT_PATH
  )
if(NOT SPDYLAY_INCLUDES)
    find_path (SPDYLAY_INCLUDES spdylay.h
      PATHS /usr/local/include /usr/include /include /sw/include /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu/ ${CMAKE_EXTRA_INCLUDES} PATH_SUFFIXES spdylay/ spdylay/include
      )
endif(NOT SPDYLAY_INCLUDES)

## -----------------------------------------------------------------------------
## Check for the library

find_library (SPDYLAY_LIBRARIES NAMES spdylay
  PATHS ${CMAKE_EXTRA_LIBRARIES} PATH_SUFFIXES spdylay/ NO_DEFAULT_PATH
  )
if(NOT SPDYLAY_LIBRARIES)
    find_library (SPDYLAY_LIBRARIES NAMES spdylay
      PATHS /usr/local/lib /usr/lib /lib /sw/lib ${CMAKE_EXTRA_LIBRARIES} PATH_SUFFIXES spdylay/
      )
endif(NOT SPDYLAY_LIBRARIES)

## -----------------------------------------------------------------------------
## Actions taken when all components have been found

if (SPDYLAY_INCLUDES AND SPDYLAY_LIBRARIES)
  set (HAVE_SPDYLAY TRUE)
else (SPDYLAY_INCLUDES AND SPDYLAY_LIBRARIES)
  if (NOT SPDYLAY_FIND_QUIETLY)
    if (NOT SPDYLAY_INCLUDES)
      message (STATUS "Unable to find SPDYLAY header files!")
    endif (NOT SPDYLAY_INCLUDES)
    if (NOT SPDYLAY_LIBRARIES)
      message (STATUS "Unable to find SPDYLAY library files!")
    endif (NOT SPDYLAY_LIBRARIES)
  endif (NOT SPDYLAY_FIND_QUIETLY)
endif (SPDYLAY_INCLUDES AND SPDYLAY_LIBRARIES)

if (HAVE_SPDYLAY)
  if (NOT SPDYLAY_FIND_QUIETLY)
    message (STATUS "Found components for SPDYLAY")
    message (STATUS "SPDYLAY_INCLUDES = ${SPDYLAY_INCLUDES}")
    message (STATUS "SPDYLAY_LIBRARIES     = ${SPDYLAY_LIBRARIES}")
  endif (NOT SPDYLAY_FIND_QUIETLY)
else (HAVE_SPDYLAY)
  if (SPDYLAY_FIND_REQUIRED)
    message (FATAL_ERROR "Could not find SPDYLAY!")
  endif (SPDYLAY_FIND_REQUIRED)
endif (HAVE_SPDYLAY)

mark_as_advanced (
  HAVE_SPDYLAY
  SPDYLAY_LIBRARIES
  SPDYLAY_INCLUDES
  )
