cmake_minimum_required(VERSION 3.16)
project(NetGreeting VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(NG_PULSE_LIBS "")
set(NG_PULSE_INCLUDES "")
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PULSE REQUIRED libpulse-simple)
  set(NG_PULSE_LIBS ${PULSE_LIBRARIES})
  set(NG_PULSE_INCLUDES ${PULSE_INCLUDE_DIRS})
endif()

set(NG_GUI_TYPE "")
if(WIN32)
  set(NG_GUI_TYPE WIN32)
elseif(APPLE)
  set(NG_GUI_TYPE MACOSX_BUNDLE)
endif()

set(NG_SOURCES
  src/main.cpp
  src/style.cpp
  src/style.h
  src/identity.h
  src/protocol.h
  src/conference.cpp
  src/conference.h
  src/landirectory.cpp
  src/landirectory.h
  src/camera.cpp
  src/camera.h
  src/audio.cpp
  src/audio.h
  src/mainwindow.cpp
  src/mainwindow.h
  src/chatwindow.cpp
  src/chatwindow.h
  src/whiteboard.cpp
  src/whiteboard.h
  src/filetransfer.cpp
  src/filetransfer.h
  src/sharewindow.cpp
  src/sharewindow.h
  src/dialogs.cpp
  src/dialogs.h
  src/videopane.cpp
  src/videopane.h
  src/greetingpostclient.cpp
  src/greetingpostclient.h
  src/resources.qrc
)

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/packaging/netgreeting.rc")
  list(APPEND NG_SOURCES packaging/netgreeting.rc)
endif()

add_executable(netgreeting ${NG_GUI_TYPE} ${NG_SOURCES})

target_include_directories(netgreeting PRIVATE src ${NG_PULSE_INCLUDES})
target_compile_definitions(netgreeting PRIVATE
  NETGREETING_VERSION="${PROJECT_VERSION}"
)
if(UNIX AND NOT APPLE)
  target_compile_definitions(netgreeting PRIVATE HAVE_PULSE)
endif()
target_link_libraries(netgreeting PRIVATE
  Qt6::Widgets
  Qt6::Network
  ${NG_PULSE_LIBS}
)

if(APPLE)
  set_target_properties(netgreeting PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "NetGreeting"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.netgreeting.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE netgreeting.icns
    MACOSX_BUNDLE_INFO_STRING "NetGreeting ${PROJECT_VERSION}"
  )
  if(EXISTS "${CMAKE_SOURCE_DIR}/packaging/netgreeting.icns")
    set_source_files_properties(packaging/netgreeting.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(netgreeting PRIVATE packaging/netgreeting.icns)
  endif()
endif()

add_executable(greetingpost
  src/greetingpost.cpp
  src/protocol.h
)
target_include_directories(greetingpost PRIVATE src)
target_compile_definitions(greetingpost PRIVATE
  NETGREETING_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(greetingpost PRIVATE Qt6::Network Qt6::Core)

include(GNUInstallDirs)

install(TARGETS netgreeting greetingpost
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
  install(FILES packaging/netgreeting.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES resources/netgreeting.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
          RENAME netgreeting.png)
  if(EXISTS "${CMAKE_SOURCE_DIR}/packaging/netgreeting.metainfo.xml")
    install(FILES packaging/netgreeting.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  endif()
endif()

# Copy Qt runtime next to the app on Windows / macOS when packaging.
if(WIN32)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
  if(WINDEPLOYQT_EXECUTABLE)
    install(CODE "
      execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations
        --release \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/netgreeting.exe\")
    ")
  endif()
elseif(APPLE)
  find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${Qt6_DIR}/../../../bin")
  if(MACDEPLOYQT_EXECUTABLE)
    install(CODE "
      execute_process(COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
        \"\${CMAKE_INSTALL_PREFIX}/netgreeting.app\" -always-overwrite)
    ")
  endif()
endif()

set(CPACK_PACKAGE_NAME "netgreeting")
set(CPACK_PACKAGE_VENDOR "Pixelhack Studios")
set(CPACK_PACKAGE_CONTACT "hi@pixelhackstudios.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Video calls, chat, whiteboard — a NetMeeting homage")
set(CPACK_PACKAGE_DESCRIPTION "NetGreeting is a desktop conferencing app: call by address, chat, shared whiteboard, file transfer, and a public directory of who is online.")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/pixelhackstudios")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")
set(CPACK_STRIP_FILES TRUE)

if(WIN32)
  set(CPACK_GENERATOR "NSIS;ZIP")
  set(CPACK_NSIS_DISPLAY_NAME "NetGreeting")
  set(CPACK_NSIS_PACKAGE_NAME "NetGreeting")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/packaging/netgreeting.ico")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\netgreeting.exe")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "NetGreeting")
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
  set(CPACK_PACKAGE_EXECUTABLES "netgreeting" "NetGreeting")
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "NetGreeting")
else()
  set(CPACK_GENERATOR "DEB;RPM")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pixelhack Studios <hi@pixelhackstudios.com>")
  set(CPACK_DEBIAN_PACKAGE_SECTION "net")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
  set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

include(CPack)
