#-------------------------------------------------
#
# Project created by QtCreator 2013-02-11T22:59:31
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = provisor
TEMPLATE = app


SOURCES += main.cpp\
        provisor.cpp \ 
    STD_SHA_256.cpp

HEADERS  += provisor.h \ 
    STD_SHA_256.h

FORMS    += provisor.ui

LIBS += -lboost_program_options \
        -lqjson
#        -lcrypto \
#        -lssl

QT += network \
      webkit
