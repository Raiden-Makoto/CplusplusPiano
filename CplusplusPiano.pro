QT += core widgets

CONFIG += c++17 sdk_no_version_check

# Core Audio (macOS native)
macx {
    LIBS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit
}

TARGET = CplusplusPiano
TEMPLATE = app

# Source files
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp

# Header files
HEADERS += \
    src/mainwindow.h

# Resources (optional - for icons, sounds, etc.)
# RESOURCES +=

# Output directory
DESTDIR = $$PWD/build
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc

# Mac-specific settings
macx {
    # ICON = 
    # QMAKE_INFO_PLIST = Info.plist
}

