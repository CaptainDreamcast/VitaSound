## This file is a quick tutorial on writing CMakeLists for targeting the Vita (adapted from the Vita SDK samples)
cmake_minimum_required(VERSION 2.8)

## This includes the Vita toolchain, must go before project definition
# It is a convenience so you do not have to type
# -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake for cmake. It is
# highly recommended that you include this block for all projects.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

## Define project parameters here
# Name of the project
project(VitaAudio)
# This line adds Vita helper macros, must go after project definition in order
# to build Vita specific artifacts (self/vpk).
include("${VITASDK}/share/vita.cmake" REQUIRED)

## Configuration options for this app
# Display name (under bubble in LiveArea)
set(VITA_APP_NAME "VitaAudio")
# Unique ID must be exactly 9 characters. Recommended: XXXXYYYYY where X =
# unique string of developer and Y = a unique number for this app
set(VITA_TITLEID  "PRSM00002")
# Optional version string to show in LiveArea's more info screen
set(VITA_VERSION  "01.00")

## Flags and includes for building
# Note that we make sure not to overwrite previous flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=c99 -I../Nogg -I../include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q -O2 -std=c++11 -I../Nogg -I../include")
# Optional. You can specify more param.sfo flags this way.
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")

# Add any additional include paths here
include_directories(
)

# Add any additional library paths here
# ${CMAKE_CURRENT_BINARY_DIR} lets you use any library currently being built
link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

## Build and link
# Add all the files needed to compile here
add_executable(${PROJECT_NAME}
  font.c
  Main.cpp
  Nogg/src/util/memory.c
  Nogg/src/util/float-to-int16.c
  Nogg/src/util/decode-frame.c
  Nogg/src/decode/stb_vorbis.c
  Nogg/src/decode/setup.c
  Nogg/src/decode/seek.c
  Nogg/src/decode/packet.c
  Nogg/src/decode/io.c
  Nogg/src/decode/decode.c
  Nogg/src/decode/crc32.c
  Nogg/src/api/version.c
  Nogg/src/api/seek-tell.c
  Nogg/src/api/read-int16.c
  Nogg/src/api/read-float.c
  Nogg/src/api/open-file.c
  Nogg/src/api/open-callbacks.c
  Nogg/src/api/open-buffer.c
  Nogg/src/api/info.c
  Nogg/src/api/close.c
  DrakonSound/DrakonWavFile.c
  DrakonSound/DrakonFileBuffer.c
  DrakonSound/DrakonSound.c
)

# Library to link to (drop the -l prefix). This will mostly be stubs.
target_link_libraries(${PROJECT_NAME}
  vita2d
  SceDisplay_stub
  SceGxm_stub 
  SceCommonDialog_stub 
  SceAppMgr_stub
  SceCtrl_stub 
  SceTouch_stub 
  SceAudio_stub 
  freetype 
  bz2 
  png 
  z 
  m 
  stdc++
)

## Create Vita files
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
# The FILE directive lets you add additional files to the VPK, the syntax is
# FILE src_path dst_path_in_vpk. In this case, we add the LiveArea paths.
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE test_assets test_assets
)
