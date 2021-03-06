#=============================================================================
#  MuseScore
#  Music Composition & Notation
#  $Id:$
#
#  Copyright (C) 2011-2013 Werner Schweer
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2
#  as published by the Free Software Foundation and appearing in
#  the file LICENSE.GPL
#=============================================================================

set(CMAKE_VERBOSE_MAKEFILE ON)
enable_testing()

include_directories(
      ${PROJECT_BINARY_DIR}
      ${PROJECT_SOURCE_DIR}
      )

if (OMR)
set(OMR_SRC ${PROJECT_SOURCE_DIR}/omr/importpdf.cpp)
endif (OMR)

QT4_WRAP_CPP (mtestMocs
      ${PROJECT_SOURCE_DIR}/mscore/qmlplugin.h
      )

add_library(
      testutils STATIC
      ${mtestMocs}
      testutils.cpp
      ${PROJECT_SOURCE_DIR}/libmscore/mcursor.cpp
      ${PROJECT_SOURCE_DIR}/mscore/bb.cpp
      ${PROJECT_SOURCE_DIR}/mscore/capella.cpp
      ${PROJECT_SOURCE_DIR}/mscore/capxml.cpp
      ${PROJECT_SOURCE_DIR}/mscore/exportxml.cpp
      ${PROJECT_SOURCE_DIR}/mscore/importmidi.cpp
      ${PROJECT_SOURCE_DIR}/mscore/exportmidi.cpp
      ${PROJECT_SOURCE_DIR}/mscore/importxml.cpp
      ${PROJECT_SOURCE_DIR}/mscore/importxmlfirstpass.cpp
      ${PROJECT_SOURCE_DIR}/mscore/musicxmlsupport.cpp
      ${PROJECT_SOURCE_DIR}/mscore/qmlplugin.cpp
      ${PROJECT_SOURCE_DIR}/mscore/shortcut.cpp
      ${PROJECT_SOURCE_DIR}/thirdparty/rtf2html/fmt_opts.cpp    # required by capella.cpp and capxml.cpp
      ${PROJECT_SOURCE_DIR}/thirdparty/rtf2html/rtf2html.cpp    # required by capella.cpp and capxml.cpp
      ${PROJECT_SOURCE_DIR}/thirdparty/rtf2html/rtf_keyword.cpp # required by capella.cpp and capxml.cpp
      ${PROJECT_SOURCE_DIR}/thirdparty/rtf2html/rtf_table.cpp   # required by capella.cpp and capxml.cpp
      ${OMR_SRC}
      omr
      )

add_executable(
      mtest
      mtest.cpp
      )

target_link_libraries(
      mtest
      ${QT_LIBRARIES}
      )

set_target_properties (
      testutils
      PROPERTIES
      COMPILE_FLAGS "-include all.h -I${QT_HEADERS_DIR} -D TESTROOT=\\\"${PROJECT_SOURCE_DIR}\\\" -g -Wall -Wextra"
      )

add_custom_target(report
      COMMAND ant -f ${PROJECT_SOURCE_DIR}/mtest/build.xml -Droot.dir=${PROJECT_BINARY_DIR}/mtest report
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/mtest"
      )

add_custom_target(reporthtml
      COMMAND ant -f ${PROJECT_SOURCE_DIR}/mtest/build.xml -Droot.dir=${PROJECT_BINARY_DIR}/mtest reporthtml
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/mtest"
      )

subdirs (libmscore musicxml importmidi capella biab)

if (OMR)
subdirs(omr)
endif (OMR)
