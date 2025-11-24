QT += core gui widgets

TEMPLATE = lib
CONFIG += plugin c++17
TARGET = CpuTempColorPlugin

INCLUDEPATH += ../../ \
               ../../i2c_smbus

SOURCES += \
    CpuTempColorPlugin.cpp

HEADERS += \
    CpuTempColorPlugin.h

DESTDIR = $$PWD/bin
