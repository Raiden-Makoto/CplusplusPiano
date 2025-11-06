QT += core widgets multimedia

CONFIG += c++17 sdk_no_version_check

TARGET = CplusplusPiano
TEMPLATE = app

# Source files
SOURCES += \
    main.cpp \
    mainwindow.cpp

# Header files
HEADERS += \
    mainwindow.h

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

