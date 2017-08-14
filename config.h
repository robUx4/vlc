/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */


#ifndef _REENTRANT
# define _REENTRANT
#endif


#if defined(_WIN32) && !defined(_WIN32_WINNT)

# define _WIN32_WINNT 0x0502 /* Windows XP SP2 */

#endif

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define to 1 to allow running VLC as root (uid 0). */
/* #undef ALLOW_RUN_AS_ROOT */

/* Maximum supported data alignment */
#define ATTRIBUTE_ALIGNED_MAX 64

/* Define to 1 if 3D Now! inline assembly is available. */
#define CAN_COMPILE_3DNOW 1

/* Define to 1 if AltiVec inline assembly is available. */
/* #undef CAN_COMPILE_ALTIVEC */

/* Define to 1 if C AltiVec extensions are available. */
/* #undef CAN_COMPILE_C_ALTIVEC */

/* Define to 1 inline MMX assembly is available. */
#define CAN_COMPILE_MMX 1

/* Define to 1 if MMX EXT inline assembly is available. */
#define CAN_COMPILE_MMXEXT 1

/* Define to 1 if SSE inline assembly is available. */
#define CAN_COMPILE_SSE 1

/* Define to 1 if SSE2 inline assembly is available. */
#define CAN_COMPILE_SSE2 1

/* Define to 1 if SSE3 inline assembly is available. */
#define CAN_COMPILE_SSE3 1

/* Define to 1 if SSE4A inline assembly is available. */
#define CAN_COMPILE_SSE4A 1

/* Define to 1 if SSE4_1 inline assembly is available. */
#define CAN_COMPILE_SSE4_1 1

/* Define to 1 if SSE4_2 inline assembly is available. */
#define CAN_COMPILE_SSE4_2 1

/* Define to 1 if SSSE3 inline assembly is available. */
#define CAN_COMPILE_SSSE3 1

/* The ./configure command line */
#define CONFIGURE_LINE "../../work/extras/package/win32/../../../configure  '--enable-update-check' '--enable-lua' '--enable-faad' '--enable-flac' '--enable-theora' '--enable-twolame' '--enable-avcodec' '--enable-merge-ffmpeg' '--enable-dca' '--enable-mpc' '--enable-libass' '--enable-x264' '--enable-schroedinger' '--enable-realrtsp' '--enable-live555' '--enable-dvdread' '--enable-shout' '--enable-goom' '--enable-caca' '--disable-sdl' '--enable-qt' '--enable-skins2' '--enable-sse' '--enable-mmx' '--enable-libcddb' '--enable-zvbi' '--disable-telx' '--enable-nls' '--build=x86_64-w64-mingw32' '--with-contrib=../../work/contrib/x86_64-w64-mingw32' '--enable-debug' '--disable-nls' '--disable-ncurses' '--disable-skins2' '--disable-caca' '--disable-chromecast' '--prefix=/c/Users/robUx4/Documents/Programs/Videolabs/build/win64/_win64' 'build_alias=x86_64-w64-mingw32' 'CFLAGS=-g -O0 -D_SYSCRT' 'CXXFLAGS=-g -O0' 'PKG_CONFIG_PATH=/mingw64/lib/pkgconfig:/mingw64/share/pkgconfig'"

/* Copyright string */
#define COPYRIGHT_MESSAGE "Copyright © 1996-2017 the VideoLAN team"

/* The copyright years */
#define COPYRIGHT_YEARS "1996-2017"

/* Default font family */
/* #undef DEFAULT_FAMILY */

/* Default font */
/* #undef DEFAULT_FONT_FILE */

/* Default monospace font family */
/* #undef DEFAULT_MONOSPACE_FAMILY */

/* Default monospace font */
/* #undef DEFAULT_MONOSPACE_FONT_FILE */

/* Binary specific version */
/* #undef DISTRO_VERSION */

/* Define if you want the addons manager modules */
#define ENABLE_ADDONMANAGERMODULES 1

/* Define if you want the HTTP daemon support */
#define ENABLE_HTTPD 1

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define to 1 for stream output support. */
#define ENABLE_SOUT 1

/* Define if you want the VideoLAN manager support */
#define ENABLE_VLM 1

/* Define to 1 if you have the <a52dec/a52.h> header file. */
#define HAVE_A52DEC_A52_H 1

/* Define to 1 if you have the `accept4' function. */
/* #undef HAVE_ACCEPT4 */

/* Define to 1 if you have alarm function */
/* #undef HAVE_ALARM */

/* Define to 1 if you have the <altivec.h> header file. */
/* #undef HAVE_ALTIVEC_H */

/* Define to 1 if you have the <ApplicationServices/ApplicationServices.h>
   header file. */
/* #undef HAVE_APPLICATIONSERVICES_APPLICATIONSERVICES_H */

/* Define if libaribb24 is available. */
#define HAVE_ARIBB24 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
/* #undef HAVE_ARPA_INET_H */

/* Define to 1 if you have asprintf function */
/* #undef HAVE_ASPRINTF */

/* Define to 1 if you have the `atof' function. */
#define HAVE_ATOF 1

/* Define to 1 if you have the `atoll' function. */
#define HAVE_ATOLL 1

/* Support for __attribute__((packed)) for structs */
/* #undef HAVE_ATTRIBUTE_PACKED */

/* Define to 1 if you have the <audio_io.h> header file. */
/* #undef HAVE_AUDIO_IO_H */

/* Define if the d3d11va module is built */
#define HAVE_AVCODEC_D3D11VA 1

/* Define to 1 if you have the `av_vda_alloc_context' function. */
/* #undef HAVE_AV_VDA_ALLOC_CONTEXT */

/* Define to 1 if you have the `backtrace' function. */
/* #undef HAVE_BACKTRACE */

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* define if the compiler supports basic C++11 syntax */
/* #undef HAVE_CXX11 */

/* Define to 1 if you have the <d3d11.h> header file. */
#define HAVE_D3D11_H 1

/* Define to 1 if you have the <d3d9.h> header file. */
#define HAVE_D3D9_H 1

/* Define to 1 if you have the `daemon' function. */
/* #undef HAVE_DAEMON */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you have the <ddraw.h> header file. */
#define HAVE_DDRAW_H 1

/* Define to 1 if you have the <DeckLinkAPIDispatch.cpp> header file. */
/* #undef HAVE_DECKLINKAPIDISPATCH_CPP */

/* Define to 1 if you have the `dirfd' function. */
/* #undef HAVE_DIRFD */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you have the <dxgidebug.h> header file. */
#define HAVE_DXGIDEBUG_H 1

/* Define to 1 if you have the <dxva2api.h> header file. */
#define HAVE_DXVA2API_H 1

#define HAVE_DYNAMIC_PLUGINS 1

/* Define to 1 if evas is enabled. */
/* #undef HAVE_EVAS */

/* Define to 1 if you have the `eventfd' function. */
/* #undef HAVE_EVENTFD */

/* Define to 1 if you have the <execinfo.h> header file. */
/* #undef HAVE_EXECINFO_H */

/* Define to 1 if you have the `fcntl' function. */
/* #undef HAVE_FCNTL */

/* Define to 1 if you have the `fdopendir' function. */
/* #undef HAVE_FDOPENDIR */

/* Define to 1 if you have the <features.h> header file. */
/* #undef HAVE_FEATURES_H */

/* Define to 1 if you have the `ffsll' function. */
/* #undef HAVE_FFSLL */

/* Define to 1 if you have the `flock' function. */
/* #undef HAVE_FLOCK */

/* Define to 1 if you have the `flockfile' function. */
/* #undef HAVE_FLOCKFILE */

/* Define to 1 if you have the <fluidlite.h> header file. */
#define HAVE_FLUIDLITE_H 1

/* Define to 1 if you have the <fontconfig/fontconfig.h> header file. */
#define HAVE_FONTCONFIG_FONTCONFIG_H 1

/* Define to 1 if you have the `fork' function. */
/* #undef HAVE_FORK */

/* Define to 1 if you have the `fstatvfs' function. */
/* #undef HAVE_FSTATVFS */

/* Define to 1 if you have the `fsync' function. */
/* #undef HAVE_FSYNC */

/* Defined if having gcrypt */
#define HAVE_GCRYPT 1

/* Define to 1 if you have the `getdelim' function. */
/* #undef HAVE_GETDELIM */

/* Define to 1 if you have the `getenv' function. */
#define HAVE_GETENV 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getpid' function. */
#define HAVE_GETPID 1

/* Define to 1 if you have the `getpwuid_r' function. */
/* #undef HAVE_GETPWUID_R */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <GL/wglew.h> header file. */
#define HAVE_GL_WGLEW_H 1

/* Define to 1 if you have gmtime_r function */
/* #undef HAVE_GMTIME_R */

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if the system has the type `ID3D11VideoDecoder'. */
#define HAVE_ID3D11VIDEODECODER 1

/* Define to 1 if you have GNU libidn. */
/* #undef HAVE_IDN */

/* Define to 1 if you have the `if_nameindex' function. */
/* #undef HAVE_IF_NAMEINDEX */

/* Define to 1 if you have the `if_nametoindex' function. */
/* #undef HAVE_IF_NAMETOINDEX */

/* Define to 1 if you have inet_pton function */
/* #undef HAVE_INET_PTON */

/* Define to 1 if you have the <interface/mmal/mmal.h> header file. */
/* #undef HAVE_INTERFACE_MMAL_MMAL_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* For FreeBSD VCD support */
/* #undef HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H */

/* Define to 1 if you have the `isatty' function. */
#define HAVE_ISATTY 1

/* Define to 1 if the system has the type `ISpObjectToken'. */
#define HAVE_ISPOBJECTTOKEN 1

/* Define to 1 if you have the <jpeglib.h> header file. */
#define HAVE_JPEGLIB_H 1

/* Define to 1 if you have the <kai.h> header file. */
/* #undef HAVE_KAI_H */

/* Define to 1 if you have the <kva.h> header file. */
/* #undef HAVE_KVA_H */

/* Define to 1 if you have the <lauxlib.h> header file. */
/* #undef HAVE_LAUXLIB_H */

/* Define to 1 if you have the <libavcodec/avcodec.h> header file. */
#define HAVE_LIBAVCODEC_AVCODEC_H 1

/* Define to 1 if you have the <libavcodec/d3d11va.h> header file. */
#define HAVE_LIBAVCODEC_D3D11VA_H 1

/* Define to 1 if you have the <libavcodec/dxva2.h> header file. */
#define HAVE_LIBAVCODEC_DXVA2_H 1

/* Define to 1 if you have the <libavcodec/vaapi.h> header file. */
/* #undef HAVE_LIBAVCODEC_VAAPI_H */

/* Define to 1 if you have the <libavcodec/vda.h> header file. */
/* #undef HAVE_LIBAVCODEC_VDA_H */

/* Define to 1 if you have the <libavformat/avformat.h> header file. */
#define HAVE_LIBAVFORMAT_AVFORMAT_H 1

/* Define to 1 if you have the <libavformat/avio.h> header file. */
#define HAVE_LIBAVFORMAT_AVIO_H 1

/* Define to 1 if you have the <libavutil/avutil.h> header file. */
#define HAVE_LIBAVUTIL_AVUTIL_H 1

/* Define to 1 if you have the <libbpg.h> header file. */
/* #undef HAVE_LIBBPG_H */

/* Define this if you have libcddb installed */
#define HAVE_LIBCDDB 1

/* Define to 1 if you have the <libcrystalhd/bc_drv_if.h> header file. */
/* #undef HAVE_LIBCRYSTALHD_BC_DRV_IF_H */

/* Define to 1 if you have the <libcrystalhd/bc_dts_defs.h> header file. */
#define HAVE_LIBCRYSTALHD_BC_DTS_DEFS_H 1

/* Define to 1 if you have the `mingw32' library (-lmingw32). */
#define HAVE_LIBMINGW32 1

/* Define to 1 if you have the <libswscale/swscale.h> header file. */
#define HAVE_LIBSWSCALE_SWSCALE_H 1

/* Define to 1 if you have the <libtar.h> header file. */
/* #undef HAVE_LIBTAR_H */

/* Define to 1 if you have the libvorbis */
#define HAVE_LIBVORBIS 1

/* Define to 1 if you have the <linux/dccp.h> header file. */
/* #undef HAVE_LINUX_DCCP_H */

/* Define to 1 if you have the <linux/magic.h> header file. */
/* #undef HAVE_LINUX_MAGIC_H */

/* Define to 1 if you have the <linux/videodev2.h> header file. */
/* #undef HAVE_LINUX_VIDEODEV2_H */

/* Define to 1 if you have the `lldiv' function. */
#define HAVE_LLDIV 1

/* Define to 1 if you have localtime_r function */
/* #undef HAVE_LOCALTIME_R */

/* Define to 1 if you have the lrintf function */
#define HAVE_LRINTF 1

/* Define to 1 if you have the `lstat' function. */
/* #undef HAVE_LSTAT */

/* Define to 1 if you have the <lualib.h> header file. */
/* #undef HAVE_LUALIB_H */

/* Define to 1 if you have the <lua.h> header file. */
/* #undef HAVE_LUA_H */

/* Define to 1 if you have the <mad.h> header file. */
#define HAVE_MAD_H 1

/* Define to 1 if the system has the type `max_align_t'. */
#define HAVE_MAX_ALIGN_T 1

/* Define to 1 if you have the `memalign' function. */
/* #undef HAVE_MEMALIGN */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memrchr' function. */
/* #undef HAVE_MEMRCHR */

/* Define to 1 if you have mkdtemp function */
/* #undef HAVE_MKDTEMP */

/* Define to 1 if you have the `mkostemp' function. */
/* #undef HAVE_MKOSTEMP */

/* Define to 1 if you have the `mmap' function. */
/* #undef HAVE_MMAP */

/* Define to 1 if MMX intrinsics are available. */
#define HAVE_MMX_INTRINSICS 1

/* Define to 1 if you have the <mntent.h> header file. */
/* #undef HAVE_MNTENT_H */

/* Define to 1 if you have the <mpcdec/mpcdec.h> header file. */
/* #undef HAVE_MPCDEC_MPCDEC_H */

/* Define to 1 if you have the <mpc/mpcdec.h> header file. */
#define HAVE_MPC_MPCDEC_H 1

/* Define to 1 if you have the NANF function */
#define HAVE_NANF 1

/* Define to 1 if you have the <neaacdec.h> header file. */
#define HAVE_NEAACDEC_H 1

/* Define to 1 if you have the <netinet/udplite.h> header file. */
/* #undef HAVE_NETINET_UDPLITE_H */

/* Define to 1 if you have the <net/if.h> header file. */
/* #undef HAVE_NET_IF_H */

/* Define to 1 if you have the `nrand48' function. */
/* #undef HAVE_NRAND48 */

/* Define to 1 if you have the `openat' function. */
/* #undef HAVE_OPENAT */

/* Define to 1 if you have the `open_memstream' function. */
/* #undef HAVE_OPEN_MEMSTREAM */

/* Define to 1 if you have pause function */
/* #undef HAVE_PAUSE */

/* Define to 1 if you have the `pipe2' function. */
/* #undef HAVE_PIPE2 */

/* Define to 1 if you have the <png.h> header file. */
#define HAVE_PNG_H 1

/* Define to 1 if you have the `poll' function. */
/* #undef HAVE_POLL */

/* Define to 1 if you have the `posix_fadvise' function. */
/* #undef HAVE_POSIX_FADVISE */

/* Define to 1 if you have the `posix_madvise' function. */
/* #undef HAVE_POSIX_MADVISE */

/* Define to 1 if you have the `posix_memalign' function. */
/* #undef HAVE_POSIX_MEMALIGN */

/* Define to 1 if you have the <postproc/postprocess.h> header file. */
/* #undef HAVE_POSTPROC_POSTPROCESS_H */

/* Define to 1 if you have the `pread' function. */
/* #undef HAVE_PREAD */

/* Define to 1 if using libprojectM 2.x */
#define HAVE_PROJECTM2 1

/* Define to 1 if you have the `pthread_condattr_setclock' function. */
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1

/* Define to 1 if you have the `pthread_cond_timedwait_monotonic_np' function.
   */
/* #undef HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC_NP */

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the <QTKit/QTKit.h> header file. */
/* #undef HAVE_QTKIT_QTKIT_H */

/* Define to 1 if you have realpath function */
/* #undef HAVE_REALPATH */

/* Define to 1 if you have the `recvmmsg' function. */
/* #undef HAVE_RECVMMSG */

/* Define to 1 if you have the `recvmsg' function. */
/* #undef HAVE_RECVMSG */

/* Define to 1 if you have the `rewind' function. */
#define HAVE_REWIND 1

/* Define to 1 if you have the `sched_getaffinity' function. */
/* #undef HAVE_SCHED_GETAFFINITY */

/* For NetBSD VCD support */
/* #undef HAVE_SCSIREQ_IN_SYS_SCSIIO_H */

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H 1

/* Define to 1 if you have the `sendmsg' function. */
/* #undef HAVE_SENDMSG */

/* Define to 1 if you have the `setenv' function. */
/* #undef HAVE_SETENV */

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the sincos function. */
#define HAVE_SINCOS 1

/* Define to 1 if you have the <SLES/OpenSLES.h> header file. */
/* #undef HAVE_SLES_OPENSLES_H */

/* Define to 1 if you have the <soundcard.h> header file. */
/* #undef HAVE_SOUNDCARD_H */

/* Define to 1 if sparkle is enabled. */
/* #undef HAVE_SPARKLE */

/* Define to 1 if SSE2 intrinsics are available. */
#define HAVE_SSE2_INTRINSICS 1

/* Define to 1 if <assert.h> defines static_assert. */
#define HAVE_STATIC_ASSERT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
/* #undef HAVE_STRCASECMP */

/* Define to 1 if you have the `strcasestr' function. */
/* #undef HAVE_STRCASESTR */

/* Define to 1 if you have the `strcoll' function and it is properly defined.
   */
#define HAVE_STRCOLL 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* For dvdnav demux support */
#define HAVE_STREAM_CB_IN_DVDNAV_H 1

/* Define to 1 if you have the `stricmp' function. */
#define HAVE_STRICMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Define to 1 if you have the `strndup' function. */
/* #undef HAVE_STRNDUP */

/* Define to 1 if you have the `strnicmp' function. */
#define HAVE_STRNICMP 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the `strnstr' function. */
/* #undef HAVE_STRNSTR */

/* Define to 1 if you have the `strptime' function. */
/* #undef HAVE_STRPTIME */

/* Define to 1 if you have the `strsep' function. */
/* #undef HAVE_STRSEP */

/* Define to 1 if you have the `strtof' function. */
#define HAVE_STRTOF 1

/* Define to 1 if you have the `strtok_r' function. */
/* #undef HAVE_STRTOK_R */

/* Define to 1 if you have the `strtoll' function. */
#define HAVE_STRTOLL 1

/* Define to 1 if the system has the type `struct pollfd'. */
/* #undef HAVE_STRUCT_POLLFD */

/* Define to 1 if the system has the type `struct timespec'. */
#define HAVE_STRUCT_TIMESPEC 1

/* Define to 1 if you have the `strverscmp' function. */
/* #undef HAVE_STRVERSCMP */

/* Define to 1 if you have the `swab' function. */
#define HAVE_SWAB 1

/* Define to 1 if you have the <sys/eventfd.h> header file. */
/* #undef HAVE_SYS_EVENTFD_H */

/* Define to 1 if you have the <sys/mount.h> header file. */
/* #undef HAVE_SYS_MOUNT_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/shm.h> header file. */
/* #undef HAVE_SYS_SHM_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #undef HAVE_SYS_SOCKET_H */

/* Define to 1 if you have the <sys/soundcard.h> header file. */
/* #undef HAVE_SYS_SOUNDCARD_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
/* #undef HAVE_SYS_UIO_H */

/* Define to 1 if you have the <sys/videoio.h> header file. */
/* #undef HAVE_SYS_VIDEOIO_H */

/* Define to 1 if you have the `tdestroy' function. */
/* #undef HAVE_TDESTROY */

/* Define to 1 if you have the `tfind' function. */
#define HAVE_TFIND 1

/* Define if libtiger is available. */
/* #undef HAVE_TIGER */

/* Define to 1 if you have the `timegm' function. */
/* #undef HAVE_TIMEGM */

/* Define to 1 if you have the `timespec_get' function. */
#define HAVE_TIMESPEC_GET 1

/* Define to 1 if vlc is built against Tizen SDK */
/* #undef HAVE_TIZEN_SDK */

/* Define to 1 if you have the <tremor/ivorbiscodec.h> header file. */
/* #undef HAVE_TREMOR_IVORBISCODEC_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <unzip.h> header file. */
/* #undef HAVE_UNZIP_H */

/* Define to 1 if you have the `uselocale' function. */
/* #undef HAVE_USELOCALE */

/* Define to 1 if you have the <valgrind/valgrind.h> header file. */
/* #undef HAVE_VALGRIND_VALGRIND_H */

/* Define to 1 if you have vasprintf function */
/* #undef HAVE_VASPRINTF */

/* Define to 1 if you have the <VideoDecodeAcceleration/VDADecoder.h> header
   file. */
/* #undef HAVE_VIDEODECODEACCELERATION_VDADECODER_H */

/* Define to 1 if you have the <VideoToolbox/VideoToolbox.h> header file. */
/* #undef HAVE_VIDEOTOOLBOX_VIDEOTOOLBOX_H */

/* Define to 1 if you have the `vmsplice' function. */
/* #undef HAVE_VMSPLICE */

/* Define to 1 if you have the <X11/Xlib.h> header file. */
#define HAVE_X11_XLIB_H 1

/* Define to 1 if you have the <xlocale.h> header file. */
/* #undef HAVE_XLOCALE_H */

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Define to 1 if you have the `_lock_file' function. */
/* #undef HAVE__LOCK_FILE */

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Dynamic object extension */
#define LIBEXT ".dll"

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to 1 if debug code should NOT be compiled */
/* #undef NDEBUG */

/* Define if <sys/param.h> defines ntohl. */
/* #undef NTOHL_IN_SYS_PARAM_H */

/* Define if you want to optimize memory usage over performance */
/* #undef OPTIMIZE_MEMORY */

/* Name of package */
#define PACKAGE "vlc"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "vlc"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "vlc 3.0.0-git"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "vlc"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "3.0.0-git"

/* version development string */
#define PACKAGE_VERSION_DEV "git"

/* version extra number */
#define PACKAGE_VERSION_EXTRA 0

/* version major number */
#define PACKAGE_VERSION_MAJOR 3

/* version minor number */
#define PACKAGE_VERSION_MINOR 0

/* version revision number */
#define PACKAGE_VERSION_REVISION 0

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 for Unicode (Wide Chars) APIs. */
#ifndef UNICODE
#define UNICODE 1
#endif

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "3.0.0-git"

/* Simple version string */
#define VERSION_MESSAGE "3.0.0-git Vetinari"

/* compiler */
#define VLC_COMPILER "gcc version 6.2.0 (Rev2, Built by MSYS2 project)"

/* user who ran configure */
#define VLC_COMPILE_BY "robUx4"

/* host which ran configure */
#define VLC_COMPILE_HOST "MatroskaOne"

/* Define to 1 if you want to build for Windows Store apps */
#define VLC_WINSTORE_APP 0

/* Define to limit the scope of <windows.h>. */
#define WIN32_LEAN_AND_MEAN /**/

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if the X Window System is missing or not being used. */
#define X_DISPLAY_MISSING 1

/* Define if the zvbi module is built */
#define ZVBI_COMPILED 1

/* ISO C, POSIX, and 4.3BSD things. */
#define _BSD_SOURCE 1

/* Define to 64 for large files support. */
#define _FILE_OFFSET_BITS 64

/* Define to 2 to get glibc warnings. */
#define _FORTIFY_SOURCE 2

/* Extensions to ISO C99 from ISO C11. */
#define _ISOC11_SOURCE 1

/* Extensions to ISO C89 from ISO C99. */
#define _ISOC99_SOURCE 1

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* IEEE Std 1003.1. */
#define _POSIX_C_SOURCE 200809L

/* IEEE Std 1003.1. */
#define _POSIX_SOURCE 1

/* ISO C, POSIX, and SVID things. */
#define _SVID_SOURCE 1

/* Same as _REENTANT for some other OSes. */
#define _THREAD_SAFE /**/

/* Define to 1 for Unicode (Wide Chars) APIs. */
#ifndef _UNICODE
#define _UNICODE 1
#endif

/* Define to '0x0600' for IE 6.0 (and shell) APIs. */
#define _WIN32_IE 0x0600

/* POSIX and XPG 7th edition */
#define _XOPEN_SOURCE 700

/* XPG things and X/Open Unix extensions. */
#define _XOPEN_SOURCE_EXTENDED 1

/* Define within the LibVLC source code tree. */
#define __LIBVLC__ /**/

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Alias fdatasync() to fsync() if missing. */
#define fdatasync fsync

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported directly.  */
/* #undef restrict */
/* Work around a bug in Sun C++: it does not support _Restrict or
   __restrict__, even though the corresponding Sun C compiler ends up with
   "#define restrict _Restrict" or "#define restrict __restrict__" in the
   previous line.  Perhaps some future version of Sun C++ will work with
   restrict; if so, hopefully it defines __RESTRICT like Sun C does.  */
#if defined __SUNPRO_CC && !defined __RESTRICT
# define _Restrict
# define __restrict__
#endif

/* Define to `sockaddr' if <sys/socket.h> does not define. */
/* #undef sockaddr_storage */

/* Define to `int' if <sys/socket.h> does not define. */
/* #undef socklen_t */

/* Define to `sa_family' if <sys/socket.h> does not define. */
/* #undef ss_family */

/* Define to `int' if <stddef.h> does not define. */
/* #undef ssize_t */

#include <vlc_fixups.h>


#if defined(_MSC_VER) && !defined(__clang__)
//# pragma fenv_access(off)
//# pragma fp_contract(on)
#elif defined(__GNUC__)
/* Not supported so far */
#else
//# pragma STDC FENV_ACCESS OFF
//# pragma STDC FP_CONTRACT ON
#endif

