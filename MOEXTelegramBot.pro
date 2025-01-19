QT = core network sql httpserver widgets gui charts

TARGET = MoexTelegramBot
TEMPLATE = app

CONFIG += c++20

VERSION = 0.1

HEADERS += \
    $$PWD/Src/buttondata.h \
    $$PWD/Src/core.h \
    $$PWD/Src/user.h \
    $$PWD/Src/users.h \
    $$PWD/Src/config.h

SOURCES += \
    $$PWD/Src/main.cpp \
    $$PWD/Src/buttondata.cpp \
    $$PWD/Src/user.cpp \
    $$PWD/Src/users.cpp \
    $$PWD/Src/core.cpp \
    $$PWD/Src/config.cpp

CONFIG += lrelease
CONFIG += embed_translations

PKGCONFIG += openssl

TRANSLATIONS += \
    MoexTelegramBot_ru_RU.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include($$PWD/../../Common/Common/Common.pri)
include($$PWD/../../Telegram/Telegram/Telegram.pri)
include($$PWD/../TradingCatCommon/TradingCatCommon.pri)
