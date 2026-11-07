#!/bin/sh
### Rozhuk Ivan 2026

# Exit on error.
set -e

# Init constans.
THIS_SCRIPT_NAME=$(readlink -nf "${0}")
export THIS_DIR=$(dirname "${THIS_SCRIPT_NAME}")
BUILD_SYS_SCRIPT_NAME="${THIS_DIR}/3rdparty/ssbs/ssbs.sh"
. "${BUILD_SYS_SCRIPT_NAME}"


APP_NAME='gtk-mixer'
APP_VERSION='1.0.5'
APP_URL='https://github.com/rozhuk-im/gtk-mixer'
APP_DESCR='GTK based volume control tool'


# Options.
#BUILD_TYPE='debug'
#ENABLE_ALSA='yes'	# Enable ALSA mixer backend [default: AUTO]
#ENABLE_OSS='yes'	# Enable OSSv3 mixer backend [default: AUTO]


BUILD_DIR="${THIS_DIR}/build-${BUILD_TYPE}"
SRC_DIR="${THIS_DIR}/src"

CFLAGS="${CFLAGS} -DHAVE_CONFIG_H"
MODULES='
gtk+-3.0'


# Global profile.
CFLAGS_OPTIONAL='
-fno-delete-null-pointer-checks'
CFLAGS_OPTIONAL_ONE_OF='
-std=c11 -std=c99'
# Hardening flags
CFLAGS_OPTIONAL="${CFLAGS_OPTIONAL}
-U_FORTIFY_SOURCE
-D_FORTIFY_SOURCE=2
-fsanitize=safe-stack
-fsanitize=cfi
-mretpoline
-fwrapv
-fPIE"
CFLAGS_OPTIONAL_ONE_OF="${CFLAGS_OPTIONAL_ONE_OF}
-fstack-protector-strong -fstack-protector-all"
LDFLAGS_OPTIONAL='
-Wl,-z,retpolineplt
-Wl,-z,relro
-Wl,-z,now
-Wl,-z,noexecstack'

# Release profile.
CFLAGS_RELEASE='-DNDEBUG'
CFLAGS_RELEASE_OPTIONAL='
-funroll-loops'

# Debug profile.
CFLAGS_DEBUG='-g3 -ggdb -O0 -DDEBUG'
CFLAGS_DEBUG_OPTIONAL='
-W
-Wall
-Weverything
-Wpointer-arith
-Wstrict-prototypes
-pedantic
-Wnull-dereference
-Wduplicated-cond
-Wimplicit-fallthrough
-Wno-cast-function-type-strict
-Wno-cast-qual
-Wno-c23-extensions
-Wno-date-time
-Wno-documentation
-Wno-documentation-unknown-command
-Wno-format-nonliteral
-Wno-padded
-Wno-implicit-void-ptr-cast
-Wno-reserved-identifier
-Wno-switch-default
-Wno-switch-enum
-Wno-unsafe-buffer-usage
-Wno-variadic-macros
-Wno-gnu-zero-variadic-macro-arguments
-Wno-zero-length-array'

# Debug + ASAN profile.
CFLAGS_DEBUG_ASAN="${CFLAGS_DEBUG} -fsanitize=address -fsanitize-recover=address"
CFLAGS_DEBUG_ASAN_OPTIONAL=${CFLAGS_DEBUG_OPTIONAL}
LDFLAGS_DEBUG_ASAN="${LDFLAGS_DEBUG} -fsanitize=address -fsanitize-recover=address"


FUNC_TO_CHECK='
reallocarray'

CONFIG_H='
eq PACKAGE_NAME		APP_NAME
eq PACKAGE_VERSION	APP_VERSION
eq PACKAGE_URL		APP_URL
eq PACKAGE_DESCRIPTION	APP_DESCR'

SRCS='
gtk-mixer-container.c
gtk-mixer-devs_combo.c
gtk-mixer-line.c
gtk-mixer-tray_icon.c
gtk-mixer-window.c
gtk-mixer.c
plugin_api.c'

AUTO_SRCS='
ENABLE_ALSA	plugin_alsa.c
ENABLE_OSS	plugin_oss3.c'


target_build() {

	echo "Building ${APP_NAME} ${APP_VERSION} ${BUILD_TYPE}"

	case "${SYSTEM_NAME}" in
	*BSD|DragonFly)
		CFLAGS="${CFLAGS} -D_BSD_SOURCE -DFREEBSD -I/usr/local/include"
		if [ "${SYSTEM_NAME}" = 'FreeBSD' ] || [ "${SYSTEM_NAME}" = 'DragonFly' ]; then
			ENABLE_OSS='yes'
		fi
		;;
	Darwin*)
		CFLAGS="${CFLAGS} -D_BSD_SOURCE -DDARWIN -I/usr/local/include"
		;;
	Linux*)
		CFLAGS="${CFLAGS} -D_GNU_SOURCE -DLINUX -D__USE_GNU=1"
		LIBS="${LIBS} -lrt"
		ENABLE_ALSA='yes'
		;;
	esac

	# Options handling.
	if [ -n "${ENABLE_ALSA}" ]; then
		echo 'Plugin: ALSA'
		CFLAGS="${CFLAGS} -DHAVE_ALSA"
		MODULES="${MODULES}
alsa"
	fi
	if [ -n "${ENABLE_OSS}" ]; then
		echo 'Plugin: OSS'
		CFLAGS="${CFLAGS} -DHAVE_OSS -DHAVE_VIRTUAL_OSS -I/usr/local/include"
		if [ "${SYSTEM_NAME}" = 'OpenBSD' ]; then
			LIBS="${LIBS} -lossaudio"
		fi
	fi

	pkg_check_modules ${MODULES}

	build_profile_appy

	if check_c_compiler_flag '-fPIE' &&
	   check_c_linker_flag '-pie'; then
		LDFLAGS="${LDFLAGS} -pie"
	fi

	check_c_functions_exists ${FUNC_TO_CHECK}

	add_required_srcs_ev_set ${AUTO_SRCS}

	rm -rf "${BUILD_DIR}"
	mkdir -p "${BUILD_DIR}"
	config_h_generate "${BUILD_DIR}/config.h" ${CONFIG_H}

	c_compiler_compile ${SRCS}
	c_compiler_link_executable "${APP_NAME}" ${SRCS}
}


build_sys_start "${@}"
