QT       -= gui
TEMPLATE = lib

DEFINES += MATCHER_LIBRARY
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += PRO_PWD=\\\"$$_PRO_FILE_PWD_\\\"

CONFIG += c++17
QMAKE_CFLAGS_ISYSTEM=

SOURCES += \
        matcher.cpp \
    bozorth3m.cpp \
    matcher_isoconverter.cpp

HEADERS += \
        matcher.h \
        matcher_global.h \ 
    matcher_config.h \
    bozorth3m.h \
    matcher_isoconverter.h


# ********************************************************************
#                           DEPENDENCIES
# ********************************************************************

#CONFIG += manjaro
CONFIG += debian

debian {
    #Suprema SDK (installed manually)
    unix:!macx: LIBS += -L/usr/local/lib/ -lUFMatcher
    INCLUDEPATH += /usr/local/include/suprema
    DEPENDPATH += /usr/local/include/suprema
}
manjaro {
    #Suprema SDK (installed manually)
    unix:!macx: LIBS += -L/usr/local/lib/ -lUFMatcher
    INCLUDEPATH += /usr/local/include/suprema
    DEPENDPATH += /usr/local/include/suprema
}


