TEMPLATE = lib 
TARGET = genericviewer 
CONFIG += qtopiamail qmfutil plugin

target.path += $$QMF_INSTALL_ROOT/plugins/viewers

# Use webkit to render mail if available
contains(QT_CONFIG,webkit){
    QT += webkit network
    DEFINES += USE_WEBKIT
}

DEPENDPATH += .

INCLUDEPATH += . ../../../libs/qmfutil \
               ../../../../../src/libraries/qtopiamail \
               ../../../../../src/libraries/qtopiamail/support

LIBS += -L../../../../../src/libraries/qtopiamail/build \
        -L../../../libs/qmfutil/build

LIBS += -F../../../../../src/libraries/qtopiamail/build \
        -F../../../libs/qmfutil/build

HEADERS += attachmentoptions.h browserwidget.h genericviewer.h

SOURCES += attachmentoptions.cpp browserwidget.cpp genericviewer.cpp

TRANSLATIONS += libgenericviewer-ar.ts \
                libgenericviewer-de.ts \
                libgenericviewer-en_GB.ts \
                libgenericviewer-en_SU.ts \
                libgenericviewer-en_US.ts \
                libgenericviewer-es.ts \
                libgenericviewer-fr.ts \
                libgenericviewer-it.ts \
                libgenericviewer-ja.ts \
                libgenericviewer-ko.ts \
                libgenericviewer-pt_BR.ts \
                libgenericviewer-zh_CN.ts \
                libgenericviewer-zh_TW.ts

include(../../../../../common.pri)