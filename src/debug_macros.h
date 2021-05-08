#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define QDEBUG qDebug()<<__PRETTY_FUNCTION__<<"| "
#endif // DEBUG_MACROS_H
