#
# Find OSS include header for Unix platforms.


if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	set(OSS_HDR_NAME "sys/soundcard.h")
elseif (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
	set(OSS_HDR_NAME "soundcard.h")
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(OSS_HDR_NAME "linux/soundcard.h")
else()
	set(OSS_HDR_NAME "machine/soundcard.h")
endif()


find_path(OSS_INCLUDE_DIR "${OSS_HDR_NAME}"
	"/usr/include" "/usr/local/include"
)


if (OSS_INCLUDE_DIR)
	set(OSS_FOUND TRUE)
	message(STATUS "Found OSS Audio")
else()
	set(OSS_FOUND)
	if (OSS_FIND_REQUIRED)
		message(FATAL_ERROR "FAILED to found Audio - REQUIRED")
	else()
		message(STATUS "Audio Disabled")
	endif()
endif()


mark_as_advanced (
	OSS_FOUND
	OSS_INCLUDE_DIR
)
