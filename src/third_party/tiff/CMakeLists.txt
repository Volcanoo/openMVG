project(libtiff)

include(CheckFunctionExists)
include(CheckIncludeFile)

check_include_file(assert.h HAVE_ASSERT_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(io.h HAVE_IO_H)
check_function_exists(jbg_newlen HAVE_JBG_NEWLEN)
check_function_exists(mmap HAVE_MMAP)
check_include_file(search.h HAVE_SEARCH_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)

if(WIN32)
  set(USE_WIN32_FILEIO 1)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tif_config.h.cmakein"
               "${CMAKE_CURRENT_BINARY_DIR}/tif_config.h" @ONLY)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ../zlib ${CMAKE_CURRENT_BINARY_DIR})

set(lib_srcs
    tif_aux.c
    tif_close.c
    tif_codec.c
    tif_color.c
    tif_compress.c
    tif_dir.c
    tif_dirinfo.c
    tif_dirread.c
    tif_dirwrite.c
    tif_dumpmode.c
    tif_error.c
    tif_extension.c
    tif_fax3.c
    tif_fax3sm.c
    tif_flush.c
    tif_getimage.c
    tif_jbig.c
    tif_jpeg_12.c
    tif_jpeg.c
    tif_luv.c
    tif_lzma.c
    tif_lzw.c
    tif_next.c
    tif_ojpeg.c
    tif_open.c
    tif_packbits.c
    tif_pixarlog.c
    tif_predict.c
    tif_print.c
    tif_read.c
    tif_strip.c
    tif_swab.c
    tif_thunder.c
    tif_tile.c
    tif_version.c
    tif_warning.c
    tif_write.c
    tif_zip.c
    tif_stream.cxx)
set(lib_headers
    t4.h
    tif_dir.h
    tif_fax3.h
    tiff.h
    tiffio.h
    tiffiop.h
    tiffvers.h
    tif_predict.h
    uvcode.h
    tiffio.hxx
    ${CMAKE_CURRENT_BINARY_DIR}/tif_config.h
    )

if(UNIX)
  list(APPEND lib_srcs tif_unix.c)
endif()

if(WIN32)
  list(APPEND lib_srcs tif_win32.c)
endif(WIN32)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNING)
endif(MSVC)

if(UNIX AND (CMAKE_COMPILER_IS_GNUCXX OR CV_ICC))
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

add_library(tiff STATIC ${lib_srcs} ${lib_headers})
target_link_libraries(tiff zlib)
SET_PROPERTY(TARGET tiff PROPERTY FOLDER OpenMVG/3rdParty)

if(TIFF_INCLUDE_INSTALL_DIR)
  set(INCLUDE_INSTALL_DIR ${TIFF_INCLUDE_INSTALL_DIR})
else()
  set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include/tiff")
endif()

install(FILES
  ${lib_headers}
  DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel
)
INSTALL(TARGETS tiff DESTINATION lib/)
