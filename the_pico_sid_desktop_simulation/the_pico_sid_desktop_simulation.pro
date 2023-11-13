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
    siddump.cpp \
    ../firmware/sid.c

HEADERS += \
    audiogenerator.h \
    mainwindow.h \
    siddump.h \
    ../firmware/sid.h \
    ../firmware/mos6581_8085_waves.h \
    ../firmware/f0_points_8580.h \
    ../firmware/f0_points_6581.h

FORMS += \
    mainwindow.ui

system(git clone https://github.com/libsidplayfp/resid)
system(mv resid/sid.cc resid/resid.cc)

exists(resid/sid.cc) {

message(ReSID found)
DEFINES += RESID_SUPPORT 1

SOURCES += \
    resid/resid.cc
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
