# ==== Qt ARM sysroot paths ====

QT_ARM_PATH = /root/ixp_sysroot/root/qt5-arm

INCLUDEPATH += \
    $$QT_ARM_PATH/include \
    $$QT_ARM_PATH/include/QtCore \
    $$QT_ARM_PATH/include/QtGui \
    $$QT_ARM_PATH/include/QtWidgets \
    $$QT_ARM_PATH/include/QtNetwork \
    $$QT_ARM_PATH/include/QtSql \
    $$QT_ARM_PATH/include/QtQml \
    $$QT_ARM_PATH/include/QtQuick \
    $$QT_ARM_PATH/include/QtQuickControls2 \
    $$QT_ARM_PATH/include/QtPositioning \
    $$QT_ARM_PATH/include/QtLocation \
    $$QT_ARM_PATH/include/QtXml \
    $$QT_ARM_PATH/include/QtTest

LIBS += -L$$QT_ARM_PATH/lib

# ==== sysroot includes (важно для Code Model) ====
INCLUDEPATH += \
    /root/ixp_sysroot/usr/include \
    /root/ixp_sysroot/usr/include/aarch64-linux-gnu

LIBS += -L/root/ixp_sysroot/usr/lib \
        -L/root/ixp_sysroot/usr/lib/aarch64-linux-gnu

# ==== OpenGL ES (если появится позже) ====
INCLUDEPATH += /root/ixp_sysroot/usr/include/GLES2
LIBS += -lGLESv2 -lEGL

# ==== чтобы Qt Creator корректно понимал defines ====
DEFINES += \
    QT_CORE_LIB \
    QT_GUI_LIB \
    QT_WIDGETS_LIB \
    QT_NETWORK_LIB \
    QT_SQL_LIB

# ==== отключаем лишние warning'и Code Model ====
QMAKE_CXXFLAGS += -std=gnu++17