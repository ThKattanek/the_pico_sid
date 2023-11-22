QT       += core gui multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audiogenerator.cpp \
    main.cpp \
    mainwindow.cpp \
    oscilloscope_widget.cpp \
    siddump.cpp

HEADERS += \
    audiogenerator.h \
    mainwindow.h \
    oscilloscope_widget.h
    siddump.h

# PIOC SID Emulation (Firmware)
SOURCES += \
    ../firmware/pico_sid.cpp \
    ../firmware/sid_voice.cpp \
    ../firmware/sid_wave.cpp \
    ../firmware/sid_envelope.cpp \
    ../firmware/sid_dac.cpp

HEADERS += \
    ../firmware/pico_sid.h \
    ../firmware/pico_sid_defs.h \
    ../firmware/sid_voice.h \
    ../firmware/sid_wave.h \
    ../firmware/sid_envelope.h \
    ../firmware/sid_dac.h \
    ../firmware/version.h

FORMS += \
    mainwindow.ui \
    oscilloscope_widget.ui \

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
