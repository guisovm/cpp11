TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += \
    chameneos.cpp

QMAKE_CXXFLAGS += \
    -std=c++0x \
    -pthread \
    -shared
