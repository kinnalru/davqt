#pragma once

#include <functional>
#include <iostream>

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QDebug>

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

class url_t : public QObject{
    Q_OBJECT
public:
    url_t() {}
    virtual ~url_t() {}
    
    QString last_value() const {
        return last_value_;
    }
    
    Q_SLOT void parse(const QString& u) {
        const QUrl url(u);
        
        if (!url.isValid()) {
            last_value_ = url.errorString();
            Q_EMIT error(last_value_);
            return;
        }
        
        if (url.scheme() == "http") {
            last_value_ = "false";
            qDebug() << last_value_;
            Q_EMIT ssl(false);
        } 
        else if (url.scheme() == "https") {
            last_value_ = "true";
            qDebug() << last_value_;
            Q_EMIT ssl(true);
        }
        else {
            last_value_ = tr("Invalid connection scheme:%1 must be \"http\" or \"https\"").arg(url.scheme());     
            qDebug() << last_value_;
            Q_EMIT error(last_value_);
            return;
        }
        
        if (url.host().isEmpty()) {
            last_value_ = tr("Invalid hostname %1").arg(url.host());     
            qDebug() << last_value_;
            Q_EMIT error(last_value_);
            return;
        }
        else {
            last_value_ = url.host();     
            qDebug() << last_value_;
            Q_EMIT hostname(last_value_);
        }
        
        last_value_ = url.path();     
        qDebug() << last_value_;
        Q_EMIT path(last_value_);

        last_value_ = QString::number(url.port());
        qDebug() << last_value_;
        Q_EMIT port(url.port());
        
        Q_EMIT ok();
        qDebug() << "OK!";
    }
    
Q_SIGNALS:
    void ssl(bool b);
    void hostname(const QString& host);
    void path(const QString& host);    
    void port(int);
    
    void error(const QString& host);
    void ok();
    
private:
    QString last_value_;
};


template <typename T>
class scoped_value
{
public:
  scoped_value(T& value, T in)
    : value_(value), out_(value)
  {
    value_ = in;
  }

  scoped_value(T& value, T in, T out)
  : value_(value), out_(out)
  {
    value_ = in;
  }

  ~scoped_value()
  {
    value_ = out_;
  }
private:
  T& value_;
  T out_;
};

