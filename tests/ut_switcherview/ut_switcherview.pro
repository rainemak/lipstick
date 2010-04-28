include(../common_top.pri)
TARGET = ut_switcherview

STYLE_HEADERS += $$SRCDIR/switcherstyle.h \
    $$SRCDIR/pagepositionindicatorstyle.h
MODEL_HEADERS += $$SRCDIR/switchermodel.h \
                 $$SRCDIR/switcherbuttonmodel.h \
                 $$SRCDIR/pagepositionindicatormodel.h

# unit test and unit
SOURCES += \
    ut_switcherview.cpp \
    $$SRCDIR/pagedviewport.cpp \
    $$SRCDIR/switcherview.cpp \
    $$SRCDIR/pagepositionindicatorview.cpp \
    $$SRCDIR/pagepositionindicator.cpp

# unit test and unit
HEADERS += \
    ut_switcherview.h \
    $$SRCDIR/switcherview.h \
    $$SRCDIR/switcher.h \
    $$SRCDIR/switcherstyle.h \
    $$SRCDIR/switcherbutton.h \
    $$SRCDIR/switcherbuttonmodel.h \
    $$SRCDIR/pagedpanning.h \
    $$SRCDIR/pagedviewport.h \
    $$SRCDIR/switchermodel.h \
    $$SRCDIR/mainwindow.h \
    $$SRCDIR/pagepositionindicator.h \
    $$SRCDIR/pagepositionindicatorview.h \
    $$SRCDIR/pagepositionindicatorstyle.h \
    $$SRCDIR/pagepositionindicatormodel.h


# service classes
SOURCES += ../stubs/stubbase.cpp \
    $$SRCDIR/windowinfo.cpp

include(../common_bot.pri)
