QT = core gui

CONFIG += console
TARGET = view-branch-diff
TEMPLATE = app

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    GitStructure.cpp \
    Console.cpp

HEADERS += \
    MainWindow.h \
    GitStructure.h \
    Console.h

FORMS += \
    MainWindow.ui

RESOURCES += \
    visual-branch-diff.qrc
