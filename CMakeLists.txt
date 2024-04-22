#
# Cmake configuration file
#

############################# INITIAL SECTION ##########################
cmake_minimum_required(VERSION 3.10)

project(gtk-mixer C)

set(PACKAGE_VERSION_MAJOR	1)
set(PACKAGE_VERSION_MINOR	0)
set(PACKAGE_VERSION_PATCH	4)

set(PACKAGE_NAME		"${PROJECT_NAME}")
set(PACKAGE_VERSION		"${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH}")
set(PACKAGE_URL			"https://github.com/rozhuk-im/gtk-mixer")
set(PACKAGE_BUGREPORT		"https://github.com/rozhuk-im/gtk-mixer/issues")
set(PACKAGE_STRING 		"${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_DESCRIPTION		"GTK based volume control tool")
set(PACKAGE_TARNAME		"${PACKAGE_NAME}-${PACKAGE_VERSION}")

############################# OPTIONS SECTION ##########################

option(ENABLE_ALSA		"Enable ALSA mixer backend [default: AUTO]"	OFF)
option(ENABLE_OSS		"Enable OSSv3 mixer backend [default: AUTO]"	OFF)


############################# INCLUDE SECTION ##########################

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCCompilerFlag)

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

find_library(PTHREAD_LIBRARY pthread)
list(APPEND CMAKE_REQUIRED_LIBRARIES ${PTHREAD_LIBRARY})

# Use the package PkgConfig to detect GTK+ headers/library files.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
add_definitions(${GTK3_CFLAGS_OTHER})
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

############################# MACRO SECTION ############################
macro(try_c_flag prop flag)
	# Try flag once on the C compiler
	check_c_compiler_flag("-Werror ${flag}" C_FLAG_${prop})
	if (C_FLAG_${prop})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	endif()
endmacro()

macro(try_linker_flag prop flag)
	# Check with the C compiler
	set(CMAKE_REQUIRED_FLAGS ${flag})
	check_c_compiler_flag(${flag} LINKER_FLAG_${prop})
	set(CMAKE_REQUIRED_FLAGS "")
	if (LINKER_FLAG_${prop})
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${flag}")
	endif()
endmacro()

macro(chk_function_exists func)
	string(TOUPPER HAVE_${func} __tmp)
	check_function_exists(${func} ${__tmp})
	if (${__tmp})
		add_definitions(-D${__tmp})
	endif()
endmacro()
############################# CONFIG SECTION ###########################

# Prefer local include dirs to system ones.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_BINARY_DIR}/src")

set(TAR "tar")

# Platform specific configuration.
if (CMAKE_SYSTEM_NAME MATCHES "^.*BSD$|DragonFly")
	add_definitions(-D_BSD_SOURCE -DFREEBSD)
	message(STATUS "Configuring for BSD system")
	include_directories("/usr/local/include/")
	set(TAR "gtar")
	if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD|DragonFly")
		set(ENABLE_OSS ON)
	endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	add_definitions(-D_BSD_SOURCE -DDARWIN)
	message(STATUS "Configuring for Darwin")
	set(TAR "gnutar")
endif()


if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions(-D_GNU_SOURCE -DLINUX -D__USE_GNU=1)
	message(STATUS "Configuring for Linux")
	if (BUILD_CPU_MODE STREQUAL "32")
		add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)
	endif()

	list(APPEND CMAKE_REQUIRED_LIBRARIES rt)
	set(ENABLE_ALSA ON)
endif()


# Plugins specific.
if (ENABLE_ALSA)
	message(STATUS "Plugin: ALSA")
	find_package(ALSA REQUIRED)
	if (ALSA_FOUND)
		add_definitions(-DHAVE_ALSA)
		include_directories(${ALSA_INCLUDE_DIRS})
		list(APPEND CMAKE_REQUIRED_LIBRARIES ${ALSA_LIBRARIES})
	endif()
endif()

if (ENABLE_OSS)
	message(STATUS "Plugin: OSS")
	find_package(OSS REQUIRED)
	if (OSS_FOUND)
		add_definitions(-DHAVE_OSS)
		include_directories(${OSS_INCLUDE_DIR})
		if (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
			list(APPEND CMAKE_REQUIRED_LIBRARIES ossaudio)
		endif()
	endif()
endif()


try_c_flag(PIPE				"-pipe")
try_c_flag(NO_DEL_NULL_PTR_CHKS		"-fno-delete-null-pointer-checks")


if (CMAKE_BUILD_TYPE MATCHES "Debug")
	# Process with warn flags.
	try_c_flag(W			"-W")
	try_c_flag(WALL			"-Wall")
	try_c_flag(WEVERYTHING		"-Weverything")
	if (NOT C_FLAG_WEVERYTHING)
		try_c_flag(WPOINTER		"-Wpointer-arith")
		try_c_flag(WSTRICT_PROTOTYPES	"-Wstrict-prototypes")
		try_c_flag(PEDANTIC		"-pedantic")
		try_c_flag(WNULL_DEREFERENCE	"-Wnull-dereference")
		try_c_flag(WDUPLICATED_COND	"-Wduplicated-cond")
		try_c_flag(WIMPLICIT_FALLTHROUGH "-Wimplicit-fallthrough")
	endif()

	try_c_flag(WCAST_FN_TYPE_STRICT	"-Wno-cast-function-type-strict")
	try_c_flag(WCAST_QUAL		"-Wno-cast-qual")
	try_c_flag(WDOCUMENTATION	"-Wno-documentation")
	try_c_flag(WDOC_UNKNOWN_CMD	"-Wno-documentation-unknown-command")
	try_c_flag(WPADDED		"-Wno-padded")
	#try_c_flag(WPOINTER_SIGN	"-Wno-pointer-sign")
	#try_c_flag(WRESERVED_ID_MACRO	"-Wno-reserved-id-macro")
	try_c_flag(WRESERVED_IDENTIFIER	"-Wno-reserved-identifier")
	#try_c_flag(WSIGN_COMPARE	"-Wno-sign-compare")
	try_c_flag(WSWITCHDEFAULT	"-Wno-switch-default")
	try_c_flag(WSWITCH_ENUM		"-Wno-switch-enum")
	#try_c_flag(WUNUSED_CONST	"-Wno-unused-const-variable")
	#try_c_flag(WUNUSED_FUNCTION	"-Wno-unused-function")
	#try_c_flag(WUNUSED_PARAM	"-Wno-unused-parameter")
	#try_c_flag(WUNUSED_VAR		"-Wno-unused-variable")
	try_c_flag(WUNSAFE_BUFFER_USAGE	"-Wno-unsafe-buffer-usage")
	#try_c_flag(WVARIADIC_MACROS	"-Wno-variadic-macros")
	#try_c_flag(WGNU_ZERO_VAR_MACRO_ARGS	"-Wno-gnu-zero-variadic-macro-arguments")
	try_c_flag(WZERO_LENGTH_ARRAY	"-Wno-zero-length-array")

	set(CMAKE_INSTALL_DO_STRIP FALSE)
	set(CMAKE_C_OPT_FLAGS "-g3 -ggdb -O0")
	message(STATUS "Adding -DDEBUG to definitions.")
	add_definitions(-DDEBUG)
else()
	set(CMAKE_INSTALL_DO_STRIP TRUE)
	message(STATUS "Adding -DNDEBUG to definitions.")
	add_definitions(-DNDEBUG)
endif()

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode.")
message(STATUS "CMAKE_INSTALL_DO_STRIP is ${CMAKE_INSTALL_DO_STRIP}.")


if (NOT "${CMAKE_C_COMPILER_ID}" MATCHES SunPro)
	try_c_flag(STD11		"-std=c11")
	if (NOT C_FLAG_STD11)
		try_c_flag(STD99	"-std=c99")
	endif()
endif()


# Hardening flags
try_c_flag(FORTIFY_SOURCE2		"-D_FORTIFY_SOURCE=2")
try_c_flag(FSTACK_PROTECTOR_ALL		"-fstack-protector-all")
try_c_flag(FSANITIZE_SAFE_STACK		"-fsanitize=safe-stack")
try_c_flag(FSANITIZE_CFI		"-fsanitize=cfi")
try_c_flag(MRETPOLINE			"-mretpoline")
try_c_flag(MFUNCTION_RETURN		"-mfunction-return=thunk")
try_c_flag(MINDIRECT_BRANCH		"-mindirect-branch=thunk")
try_c_flag(FWRAPV			"-fwrapv")
try_c_flag(FPIE				"-fPIE")
if (C_FLAG_FPIE)
	try_linker_flag(PIE		"-pie")
endif()
try_linker_flag(RETPOLINEPLT		"-Wl,-z,retpolineplt")
try_linker_flag(ZRELRO			"-Wl,-z,relro")
try_linker_flag(ZNOW			"-Wl,-z,now")
try_linker_flag(ZNOEXECSTACK		"-Wl,-z,noexecstack")



# Check platform API.
chk_function_exists(reallocarray)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_OPT_FLAGS}")
# Silently strip whitespace
string(STRIP "${CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
string(STRIP "${CMAKE_EXE_LINKER_FLAGS}" CMAKE_EXE_LINKER_FLAGS)
string(STRIP "${CMAKE_MODULE_LINKER_FLAGS}" CMAKE_MODULE_LINKER_FLAGS)
string(STRIP "${CMAKE_SHARED_LINKER_FLAGS}" CMAKE_SHARED_LINKER_FLAGS)

configure_file(config.h.cmake src/config.h)
add_definitions(-DHAVE_CONFIG_H)

################################ SUBDIRS SECTION #######################

add_subdirectory(src)

############################ TARGETS SECTION ###########################

add_custom_target(dist ${CMAKE_CURRENT_SOURCE_DIR}/dist.sh
	"${CMAKE_BINARY_DIR}/${PACKAGE_TARNAME}.tar.xz" "${TAR}"
	COMMENT "Create source distribution"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

##################### INSTALLATION #####################################

