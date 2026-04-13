QT       += core gui
QT       += serialport
QT       += websockets
QT       += widgets quickwidgets
QT       += core gui serialport websockets

QML_IMPORT_PATH = $$PWD/QML
RESOURCES += resources.qrc

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
INCLUDEPATH += /root/ixp_sysroot/root/qt5-arm/include \
               /root/ixp_sysroot/root/qt5-arm/include/QtCore \
               /root/ixp_sysroot/root/qt5-arm/include/QtGui \
               /root/ixp_sysroot/root/qt5-arm/include/QtWidgets
LIBS += -L/root/ixp_sysroot/usr/lib -lssl -lcrypto

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
    ModbusMaster.cpp \
    WSClient.cpp \
    device.cpp \
    device_manager.cpp \
    device_poller.cpp \
    main.cpp \
    mainwindow.cpp \
    nta8a01_device.cpp \
    relay_device.cpp


HEADERS += \
    ModbusMaster.h \
    WSClient.h \
    device.h \
    device_manager.h \
    device_poller.h \
    mainwindow.h \
    nta8a01_device.h \
    relay_device.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# ==== FIX для Qt Creator (Clang Code Model) ====

# sysroot (самое главное)
QMAKE_CFLAGS   += --sysroot=/root/ixp_sysroot
QMAKE_CXXFLAGS += --sysroot=/root/ixp_sysroot
QMAKE_SYSROOT = /root/ixp_sysroot
# стандартные include от gcc (чтобы нашёл stddef.h)
INCLUDEPATH += \
    /root/ixp_sysroot/usr/include \
    /root/ixp_sysroot/usr/include/aarch64-linux-gnu

# для clang (Qt Creator)
#QMAKE_CXXFLAGS += \
#    -isystem /usr/lib/gcc-cross/aarch64-linux-gnu/9/include \
#    -isystem /usr/aarch64-linux-gnu/include \
#    -isystem /root/ixp_sysroot/usr/include

QMAKE_CXXFLAGS += -Wno-deprecated-copy

LIBS += -L/root/ixp_sysroot/usr/lib -lssl -lcrypto

include(qt_arm_fix.pri)

DISTFILES += \
    Speedometer.qml