warning(Build with qmake is deprecated. It\'s recommended to use CMake)

QT += core network
QT -= gui

TARGET = extrachain-console
CONFIG -= app_bundle
CONFIG += c++2a console

DEFINES += ECONSOLE
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += headers

SOURCES += \
    main.cpp \
    sources/console/console_manager.cpp \
    sources/console/console_input.cpp \
    sources/console/push_manager.cpp

HEADERS += \
    headers/console/console_manager.h \
    headers/console/console_input.h \
    headers/console/push_manager.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

GIT_COMMIT = $$system(git --git-dir .git --work-tree $$PWD describe --always --tags)
GIT_BRANCH = $$system(git --git-dir .git --work-tree $$PWD symbolic-ref --short HEAD)

TARGET_LINK_LIBRARES +=
include(../extrachain-core/extrachain-core.pri)
