QT += core gui qml quick quickcontrols2 widgets svg dbus network

CONFIG += c++17 release
TARGET = spot
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
