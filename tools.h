#pragma once

#include <functional>
#include <iostream>

#include <QObject>
#include <QTimer>

#if !defined(Q_VERIFY)
#  ifndef QT_NO_DEBUG
#    define Q_VERIFY(cond) Q_ASSERT(cond)
#  else
#    define Q_VERIFY(cond) ((!(cond)) ? qt_noop() : qt_noop())
#  endif
#endif

namespace detail {
    class connect_simple_helper : public QObject {
        Q_OBJECT
    public:
        connect_simple_helper(QObject *parent, const std::function<void()> &f) : QObject(parent), f_(f), del_(false) {}

    public Q_SLOTS:
        void signaled() { f_(); if (del_) {disconnect(this); deleteLater();} }

    public:
        std::function<void()> f_;
        bool del_;
    };
}

template <class T>
bool connect(QObject *sender, const char *signal, const T &reciever, Qt::ConnectionType type = Qt::AutoConnection) {
    std::function<void()> f = reciever;
    return QObject::connect(sender, signal, new detail::connect_simple_helper(sender, f), SLOT(signaled()), type);
}

template <class T>
bool connectOnce(QObject *sender, const char *signal, const T &reciever, Qt::ConnectionType type = Qt::AutoConnection) {
    std::function<void()> f = reciever;
    auto h = new detail::connect_simple_helper(sender, f); h->del_ = true;
    return QObject::connect(sender, signal, h, SLOT(signaled()), type);
}

template <class T>
void singleShot(int ms, const T &reciever, Qt::ConnectionType type = Qt::AutoConnection) {
    std::function<void()> f = reciever;
    auto h =new detail::connect_simple_helper(0, f); h->del_ = true;
    QTimer::singleShot(ms, h, SLOT(signaled()));
}

