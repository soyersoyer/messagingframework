TEMPLATE = lib 
CONFIG += warn_on
CONFIG += qtopiamail
TARGET = qmfutil 

target.path += $$QMF_INSTALL_ROOT/lib

DEFINES += QMFUTIL_INTERNAL
symbian: {
	MMP_RULES += EXPORTUNFROZEN
}

DEPENDPATH += .

INCLUDEPATH += . ../../../../src/libraries/qtopiamail ../../../../src/libraries/qtopiamail/support

macx:LIBS += -F../../../../libraries/qtopiamail/build
LIBS += -L../../../../src/libraries/qtopiamail/build

HEADERS += emailfoldermodel.h \
           emailfolderview.h \
           folderdelegate.h \
           foldermodel.h \
           folderview.h \
           qmailcomposer.h \
           qmailviewer.h \
           selectfolder.h \
           qtmailnamespace.h

SOURCES += emailfoldermodel.cpp \
           emailfolderview.cpp \
           folderdelegate.cpp \
           foldermodel.cpp \
           folderview.cpp \
           qmailcomposer.cpp \
           qmailviewer.cpp \
           selectfolder.cpp \
           qtmailnamespace.cpp

TRANSLATIONS += libqmfutil-ar.ts \
                libqmfutil-de.ts \
                libqmfutil-en_GB.ts \
                libqmfutil-en_SU.ts \
                libqmfutil-en_US.ts \
                libqmfutil-es.ts \
                libqmfutil-fr.ts \
                libqmfutil-it.ts \
                libqmfutil-ja.ts \
                libqmfutil-ko.ts \
                libqmfutil-pt_BR.ts \
                libqmfutil-zh_CN.ts \
                libqmfutil-zh_TW.ts

RESOURCES += qmfutil.qrc

include(../../../../common.pri)
