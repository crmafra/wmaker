# wm_imgfmt_check.m4 - Macros to check for image file format support libraries
#
# Copyright (c) 2013 Christophe CURIS
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# WM_IMGFMT_CHECK_GIF
# -------------------
#
# Check for GIF file support through 'libgif', 'libungif' or 'giflib v5'
# The check depends on variable 'enable_gif' being either:
#   yes  - detect, fail if not found
#   no   - do not detect, disable support
#   auto - detect, disable if not found
#
# When found, append appropriate stuff in GFXLIBS, and append info to
# the variable 'supported_gfx'
# When not found, append info to variable 'unsupported'
AC_DEFUN_ONCE([WM_IMGFMT_CHECK_GIF],
[AC_REQUIRE([_WM_LIB_CHECK_FUNCTS])
AS_IF([test "x$enable_gif" = "xno"],
    [unsupported="$unsupported GIF"],
    [AC_CACHE_CHECK([for GIF support library], [wm_cv_imgfmt_gif],
        [wm_cv_imgfmt_gif=no
         wm_save_LIBS="$LIBS"
         dnl
         dnl We check first if one of the known libraries is available
         for wm_arg in "-lgif" "-lungif" ; do
           AS_IF([wm_fn_lib_try_link "DGifOpenFileName" "$XLFLAGS $XLIBS $wm_arg"],
             [wm_cv_imgfmt_gif="$wm_arg" ; break])
         done
         LIBS="$wm_save_LIBS"
         AS_IF([test "x$enable_gif$wm_cv_imgfmt_gif" = "xyesno"],
           [AC_MSG_ERROR([explicit GIF support requested but no library found])])
         AS_IF([test "x$wm_cv_imgfmt_gif" != "xno"],
           [dnl
            dnl A library was found, now check for the appropriate header
            wm_save_CFLAGS="$CFLAGS"
            AS_IF([wm_fn_lib_try_compile "gif_lib.h" "" "return 0" ""],
              [],
              [AC_MSG_ERROR([found $wm_cv_imgfmt_gif but could not find appropriate header - are you missing libgif-dev package?])])
            AS_IF([wm_fn_lib_try_compile "gif_lib.h" 'const char *filename = "dummy";' "DGifOpenFileName(filename)" ""],
              [wm_cv_imgfmt_gif="$wm_cv_imgfmt_gif version:4"],
              [AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM(
                  [@%:@include <gif_lib.h>

const char *filename = "dummy";],
                  [  int error_code;
  DGifOpenFileName(filename, &error_code);] )],
                [wm_cv_imgfmt_gif="$wm_cv_imgfmt_gif version:5"],
                [AC_MSG_ERROR([found $wm_cv_imgfmt_gif and header, but cannot compile - unsupported version?])])dnl
              ]
            )
            CFLAGS="$wm_save_CFLAGS"])
         ])
    AS_IF([test "x$wm_cv_imgfmt_gif" = "xno"],
        [unsupported="$unsupported GIF"
         enable_gif="no"],
        [supported_gfx="$supported_gfx GIF"
         WM_APPEND_ONCE([`echo "$wm_cv_imgfmt_gif" | sed -e 's, *version:.*,,' `], [GFXLIBS])
         AC_DEFINE_UNQUOTED([USE_GIF],
           [`echo "$wm_cv_imgfmt_gif" | sed -e 's,.*version:,,' `],
           [defined when valid GIF library with header was found])])
    ])
AM_CONDITIONAL([USE_GIF], [test "x$enable_gif" != "xno"])dnl
]) dnl AC_DEFUN


# WM_IMGFMT_CHECK_JPEG
# --------------------
#
# Check for JPEG file support through 'libjpeg'
# The check depends on variable 'enable_jpeg' being either:
#   yes  - detect, fail if not found
#   no   - do not detect, disable support
#   auto - detect, disable if not found
#
# When found, append appropriate stuff in GFXLIBS, and append info to
# the variable 'supported_gfx'
# When not found, append info to variable 'unsupported'
AC_DEFUN_ONCE([WM_IMGFMT_CHECK_JPEG],
[WM_LIB_CHECK([JPEG], [-ljpeg], [jpeg_destroy_compress], [$XLFLAGS $XLIBS],
    [AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM(
                    [@%:@include <stdlib.h>
@%:@include <stdio.h>
@%:@include <jpeglib.h>],
                    [  struct jpeg_decompress_struct cinfo;

  jpeg_destroy_decompress(&cinfo);])],
                [],
                [AS_ECHO([failed])
                 AS_ECHO(["$as_me: error: found $CACHEVAR but cannot compile header"])
                 AS_ECHO(["$as_me: error:   - does header 'jpeglib.h' exists? (is package 'jpeg-dev' missing?)"])
                 AS_ECHO(["$as_me: error:   - version of header is not supported? (report to dev team)"])
                 AC_MSG_ERROR([JPEG library is not usable, cannot continue])])
           ],
    [supported_gfx], [GFXLIBS])dnl
]) dnl AC_DEFUN


# WM_IMGFMT_CHECK_PNG
# -------------------
#
# Check for PNG file support through 'libpng'
# The check depends on variable 'enable_png' being either:
#   yes  - detect, fail if not found
#   no   - do not detect, disable support
#   auto - detect, disable if not found
#
# When found, append appropriate stuff in GFXLIBS, and append info to
# the variable 'supported_gfx'
# When not found, append info to variable 'unsupported'
AC_DEFUN_ONCE([WM_IMGFMT_CHECK_PNG],
[WM_LIB_CHECK([PNG], ["-lpng" "-lpng -lz" "-lpng -lz -lm"], [png_get_valid], [$XLFLAGS $XLIBS],
    [wm_save_CFLAGS="$CFLAGS"
     AS_IF([wm_fn_lib_try_compile "png.h" "" "return 0" ""],
         [],
         [AC_MSG_ERROR([found $CACHEVAR but could not find appropriate header - are you missing libpng-dev package?])])
     AS_IF([wm_fn_lib_try_compile "png.h" "" "png_get_valid(NULL, NULL, PNG_INFO_tRNS)" ""],
         [],
         [AC_MSG_ERROR([found $CACHEVAR and header, but cannot compile - unsupported version?])])
     CFLAGS="$wm_save_CFLAGS"],
    [supported_gfx], [GFXLIBS])dnl
]) dnl AC_DEFUN


# WM_IMGFMT_CHECK_TIFF
# --------------------
#
# Check for TIFF file support through 'libtiff'
# The check depends on variable 'enable_tiff' being either:
#   yes  - detect, fail if not found
#   no   - do not detect, disable support
#   auto - detect, disable if not found
#
# When found, append appropriate stuff in GFXLIBS, and append info to
# the variable 'supported_gfx'
# When not found, append info to variable 'unsupported'
AC_DEFUN_ONCE([WM_IMGFMT_CHECK_TIFF],
[WM_LIB_CHECK([TIFF],
    ["-ltiff"  \
     dnl TIFF can have a dependancy over zlib
     "-ltiff -lz" "-ltiff -lz -lm"  \
     dnl It may also have a dependancy to jpeg_lib
     "-ltiff -ljpeg" "-ltiff -ljpeg -lz" "-ltiff -ljpeg -lz -lm"  \
     dnl There is also a possible dependancy on JBIGKit
     "-ltiff -ljpeg -ljbig -lz"  \
     dnl Probably for historical reasons?
     "-ltiff34" "-ltiff34 -ljpeg" "-ltiff34 -ljpeg -lm"],
    [TIFFGetVersion], [$XLFLAGS $XLIBS],
    [wm_save_CFLAGS="$CFLAGS"
     AS_IF([wm_fn_lib_try_compile "tiffio.h" "" "return 0" ""],
         [],
         [AC_MSG_ERROR([found $CACHEVAR but could not find appropriate header - are you missing libtiff-dev package?])])
     AS_IF([wm_fn_lib_try_compile "tiffio.h" 'const char *filename = "dummy";' 'TIFFOpen(filename, "r")' ""],
         [],
         [AC_MSG_ERROR([found $CACHEVAR and header, but cannot compile - unsupported version?])])
     CFLAGS="$wm_save_CFLAGS"],
    [supported_gfx], [GFXLIBS])dnl
]) dnl AC_DEFUN


# WM_IMGFMT_CHECK_XPM
# -------------------
#
# Check for XPM file support through 'libXpm'
# The check depends on variable 'enable_xpm' being either:
#   yes  - detect, fail if not found
#   no   - do not detect, use internal support
#   auto - detect, use internal if not found
#
# When found, append appropriate stuff in GFXLIBS, and append info to
# the variable 'supported_gfx'
AC_DEFUN_ONCE([WM_IMGFMT_CHECK_XPM],
[AC_REQUIRE([_WM_LIB_CHECK_FUNCTS])
AS_IF([test "x$enable_xpm" = "xno"],
    [supported_gfx="$supported_gfx builtin-XPM"],
    [AC_CACHE_CHECK([for XPM support library], [wm_cv_imgfmt_xpm],
        [wm_cv_imgfmt_xpm=no
         dnl
         dnl We check first if one of the known libraries is available
         wm_save_LIBS="$LIBS"
         AS_IF([wm_fn_lib_try_link "XpmCreatePixmapFromData" "$XLFLAGS $XLIBS -lXpm"],
           [wm_cv_imgfmt_xpm="-lXpm" ; break])
         LIBS="$wm_save_LIBS"
         AS_IF([test "x$enable_xpm$wm_cv_imgfmt_xpm" = "xyesno"],
           [AC_MSG_ERROR([explicit libXpm support requested but no library found])])
         AS_IF([test "x$wm_cv_imgfmt_xpm" != "xno"],
           [dnl
            dnl A library was found, now check for the appropriate header
            wm_save_CFLAGS="$CFLAGS"
            AS_IF([wm_fn_lib_try_compile "X11/xpm.h" "" "return 0" "$XCFLAGS"],
              [],
              [AC_MSG_ERROR([found $wm_cv_imgfmt_xpm but could not find appropriate header - are you missing libXpm-dev package?])])
            AS_IF([wm_fn_lib_try_compile "X11/xpm.h" 'char *filename = "dummy";' 'XpmReadFileToXpmImage(filename, NULL, NULL)' "$XCFLAGS"],
              [],
              [AC_MSG_ERROR([found $wm_cv_imgfmt_xpm and header, but cannot compile - unsupported version?])])
            CFLAGS="$wm_save_CFLAGS"])
         ])
    AS_IF([test "x$wm_cv_imgfmt_xpm" = "xno"],
        [supported_gfx="$supported_gfx builtin-XPM"
         enable_xpm="no"],
        [supported_gfx="$supported_gfx XPM"
         WM_APPEND_ONCE([$wm_cv_imgfmt_xpm], [GFXLIBS])
         AC_DEFINE([USE_XPM], [1],
           [defined when valid XPM library with header was found])])
    ])
AM_CONDITIONAL([USE_XPM], [test "x$enable_xpm" != "xno"])dnl
]) dnl AC_DEFUN
