TARGET = QScxmlLib
MODULE = qscxmllib

QT       += core core-private qml

load(qt_module)

CONFIG   += c++11
DEFINES  += SCXML_LIBRARY \
            QT_NO_CAST_FROM_ASCII

HEADERS += \
    scxmlparser.h \
    scxmlstatetable.h \
    scxmlglobals.h \
    scxmldumper.h \
    scxmlstatetable_p.h \
    scxmlcppdumper.h \
    nulldatamodel.h \
    ecmascriptdatamodel.h \
    ecmascriptplatformproperties.h

SOURCES += \
    scxmlparser.cpp \
    scxmlstatetable.cpp \
    scxmldumper.cpp \
    scxmlcppdumper.cpp \
    nulldatamodel.cpp \
    ecmascriptdatamodel.cpp \
    ecmascriptplatformproperties.cpp
