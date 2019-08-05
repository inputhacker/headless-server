m4_define([hsvr_major], 0)
m4_define([hsvr_minor], 0)
m4_define([hsvr_micro], 0)

m4_define([hsvr_version], [hsvr_major.hsvr_minor.hsvr_micro])

AC_PREREQ([2.64])
AC_INIT([pepper], [pepper_version], [tkq.kim@samsung.com])

AC_SUBST([HSVR_VERSION_MAJOR], [hsvr_major_version])
AC_SUBST([HSVR_VERSION_MINOR], [hsvr_minor_version])
AC_SUBST([HSVR_VERSION_MICRO], [hsvr_micro_version])
AC_SUBST([HSVR_VERSION], [hsvr_version])

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CC

LT_PREREQ([2.2])
LT_INIT([disable-static])

if test "x$GCC" = "xyes"; then
GCC_CFLAGS="-Wall -Wextra -Wno-unused-parameter \
            -Wno-missing-field-initializers -g -fvisibility=hidden \
            -Wstrict-prototypes -Wmissing-prototypes -Wsign-compare"
fi
AC_SUBST(GCC_CFLAGS)

# headless server
HEADLESS_SERVER_REQUIRES="pepper pepper-inotify pepper-keyrouter pepper-devicemgr pepper-xkb pepper-evdev xkbcommon capi-system-peripheral-io xdg-shell-unstable-v6-server tizen-extension-server wayland-tbm-server"
PKG_CHECK_MODULES(HEADLESS_SERVER, $[HEADLESS_SERVER_REQUIRES])

AC_SUBST(HEADLESS_SERVER_CFLAGS)
AC_SUBST(HEADLESS_SERVER_LIBS)

# Output files
AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_OUTPUT
