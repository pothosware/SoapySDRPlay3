########################################################################
# Build Soapy SDR support module for SDRplay (API version 3)
########################################################################
cmake_minimum_required(VERSION 3.13)
project(SoapySDRPlay CXX)

find_package(SoapySDR "0.4.0" NO_MODULE REQUIRED)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package(LibSDRplay)

if (NOT LIBSDRPLAY_FOUND)
    message(FATAL_ERROR "SDRplay development files not found...")
endif ()
message(STATUS "LIBSDRPLAY_INCLUDE_DIRS - ${LIBSDRPLAY_INCLUDE_DIRS}")
message(STATUS "LIBSDRPLAY_LIBRARIES - ${LIBSDRPLAY_LIBRARIES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LIBSDRPLAY_INCLUDE_DIRS})

# As SoapySDR requires this, we can safely
# do the same
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)
    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)
endif(CMAKE_COMPILER_IS_GNUCXX)

# Configurable feature set
SET (STREAMING_USB_MODE_BULK OFF CACHE BOOL "Use USB bulk mode instead of isochronous")

IF(STREAMING_USB_MODE_BULK)
    ADD_DEFINITIONS( -DSTREAMING_USB_MODE_BULK=1 )
ENDIF()

# Gain modes
SET (GAIN_MODE "Legacy" CACHE STRING "Gain mode selection")
SET_PROPERTY(CACHE GAIN_MODE PROPERTY STRINGS Legacy SDRplay LNA DB Normalized)
SET (GAIN_MODE_IF_AGC_AS_SETTING "False" CACHE BOOL "Enable/disable IF AGC via a SoapySDR setting")
SET (GAIN_MODE_LEGACY_GENERIC_GAIN "None" CACHE STRING "Generic gain formats for legacy mode")
SET_PROPERTY(CACHE GAIN_MODE_LEGACY_GENERIC_GAIN PROPERTY STRINGS None RFGRIFGR RFGR.IFGR)

ADD_DEFINITIONS( -DGAIN_MODE_${GAIN_MODE} )
IF(GAIN_MODE STREQUAL "LNA" OR GAIN_MODE STREQUAL "DB" OR GAIN_MODE STREQUAL "Normalized")
    SET(GAIN_MODE_USE_RF_GAIN_TABLES "TRUE")
    ADD_DEFINITIONS( -DGAIN_MODE_USE_RF_GAIN_TABLES )
    IF(GAIN_MODE_IF_AGC_AS_SETTING)
        ADD_DEFINITIONS( -DGAIN_MODE_IF_AGC_AS_SETTING )
    ENDIF()
ENDIF()
IF(GAIN_MODE STREQUAL "SDRplay")
    ADD_DEFINITIONS( -DGAIN_MODE_HAS_SET_GAIN )
ENDIF()
IF(GAIN_MODE STREQUAL "LNA")
    ADD_DEFINITIONS( -DGAIN_MODE_LNA_POSITIVE -DGAIN_MODE_IF_OFFSET_DB=79 )
ENDIF()
IF(GAIN_MODE STREQUAL "DB")
    ADD_DEFINITIONS( -DGAIN_MODE_DB_POSITIVE -DGAIN_MODE_IF_OFFSET_DB=79 -DGAIN_MODE_HAS_SET_GAIN -DGAIN_MODE_HAS_GET_GAIN )
ENDIF()
IF(GAIN_MODE STREQUAL "Normalized")
    ADD_DEFINITIONS( -DGAIN_MODE_HAS_SET_GAIN -DGAIN_MODE_HAS_GET_GAIN )
ENDIF()
IF(GAIN_MODE STREQUAL "Legacy")
    IF(GAIN_MODE_LEGACY_GENERIC_GAIN STREQUAL "RFGRIFGR")
        ADD_DEFINITIONS( -DGAIN_MODE_LEGACY_GENERIC_GAIN=1 -DGAIN_MODE_HAS_SET_GAIN -DGAIN_MODE_HAS_GET_GAIN )
    ELSEIF(GAIN_MODE_LEGACY_GENERIC_GAIN STREQUAL "RFGR.IFGR")
        ADD_DEFINITIONS( -DGAIN_MODE_LEGACY_GENERIC_GAIN=2 -DGAIN_MODE_HAS_SET_GAIN -DGAIN_MODE_HAS_GET_GAIN )
    ENDIF()
ENDIF()


SET (SOURCES
    SoapySDRPlay.hpp
    Registration.cpp
    sdrplay_api.cpp
    Settings.cpp
    Streaming.cpp
    Gain${GAIN_MODE}.cpp
)
IF(GAIN_MODE_USE_RF_GAIN_TABLES)
    LIST(APPEND SOURCES GainLNAstateTables.cpp)
ENDIF()


SOAPY_SDR_MODULE_UTIL(
    TARGET sdrPlaySupport
    SOURCES
        ${SOURCES}
    LIBRARIES
        ${LIBSDRPLAY_LIBRARIES}
)

# gcc 10+ on Linux needs -pthread for pthread_cond_clockwait()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 10)
    target_link_options(sdrPlaySupport PRIVATE -pthread)
endif ()

########################################################################
# uninstall target
########################################################################
add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

