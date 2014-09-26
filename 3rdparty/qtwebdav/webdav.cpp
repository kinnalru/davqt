/* This file is part of QWebdav
 *
 * Copyright (C) 2009-2010 Corentin Chary <corentin.chary@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QDomDocument>
#include <QDomNode>
#include <QUrl>
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QAuthenticator>
#include <QBuffer>
#include <QStringList>
#include <QEventLoop>
#include <QPointer>

#include <stdexcept>

#include "webdav.h"
#include "webdav_url_info.h"


enum Property {
    ETAG = 0,
    LENGTH,
    LANGUAGE,
    CONTENTYTPE,
    CREATION,
    MODIFIED,
    TYPE,
    DISPLAY,
    SOURCE,
    EXECUTE,
    PERMISSIONS,
    END
};

static const QMap<Property, QString> properties = [] {
  QMap<Property, QString> v;
  v[ETAG]       = "getetag";
  v[LENGTH]     = "getcontentlength";
  v[LANGUAGE]   = "getcontentlanguage";
  v[CONTENTYTPE]= "getcontenttype";
  v[CREATION]   = "creationdate";
  v[MODIFIED]   = "getlastmodified";
  v[TYPE]       = "resourcetype";
  v[DISPLAY]    = "displayname";
  v[SOURCE]     = "source";
  v[EXECUTE]    = "executable";
  v[PERMISSIONS]= "permissions";
  return v;
}();

QWebdav::QWebdav(const QUrl& url, QObject* parent, QWebdav::Type type)
  : QNetworkAccessManager(parent)
  , url_(url)
  , type_(type)
{
  url_.setPath("");
  qDebug() << "QWebdav: " << url;
  connect(this, SIGNAL(finished(QNetworkReply*)), this, SLOT(int_finished(QNetworkReply*)));
  connect(this, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(authenticate(QNetworkReply*,QAuthenticator*)));
}

QWebdav::~QWebdav ()
{
}

QWebdavUrlInfo QWebdav::info(const QString& dir)
{
  QWebdav::PropNames query;
  
  query["DAV:"] = properties.values();
  query[""]     = properties.values();
  
  query["http://apache.org/dav/props/"] = QStringList() << properties[EXECUTE];
  query["DAVQT:"] = QStringList() << properties[PERMISSIONS];
  
  QList<QWebdavUrlInfo> infos(parse(propfind(dir, query, 0)));
  Q_ASSERT(infos.size() == 1);
  return infos.first();
}

QNetworkReply* QWebdav::setPermissions(const QString& path, int perms)
{
  PropValues values;
  QMap<QString, QVariant> data;
  data[properties[PERMISSIONS]] = perms;
  values["DAVQT:"] = data;
  
  qDebug() << "Webdav: set permission:" << perms;
  qDebug() << values;
  
  return proppatch(path, values);
}


QNetworkReply* QWebdav::list(const QString & dir)
{
  QWebdav::PropNames query;
  
  query["DAV:"] = properties.values();
  query[""]     = properties.values();
  
  query["http://apache.org/dav/props/"] = QStringList() << properties[EXECUTE];
  query["DAVQT:"] = QStringList() << properties[PERMISSIONS];
  
  return propfind(dir, query, 1);
}

QNetworkReply* QWebdav::search(const QString& path, const QString & q)
{
  QByteArray query = "<?xml version=\"1.0\"?>\r\n";

  query.append( "<D:searchrequest xmlns:D=\"DAV:\">\r\n" );
  query.append( q.toUtf8() );
  query.append( "</D:searchrequest>\r\n" );

  QNetworkRequest req;
  req.setUrl(mkurl(path));

  return davRequest("SEARCH", req);
}

QNetworkReply* QWebdav::put(const QString& path, QByteArray& data)
{
  QBuffer* buffer = new QBuffer();
  buffer->open(QIODevice::WriteOnly);
  QNetworkReply* reply = put(path, buffer);
  connect(reply, SIGNAL(finished()), buffer, SLOT(deleteLater()));
  return reply;
}

QNetworkReply* QWebdav::put(const QString& path, QIODevice* data)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));

  QNetworkReply* reply = davRequest("PUT", req, data);
  connect(reply, SIGNAL(uploadProgress(qint64,qint64)), this, SIGNAL(progress(qint64,qint64)));
  return reply;
}

QNetworkReply* QWebdav::get(const QString& path, QIODevice* device)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));
  
  QNetworkReply* reply = QNetworkAccessManager::get(req);
  reply->setProperty("iodevice", reinterpret_cast<quint64>(device));
  connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(progress(qint64,qint64)));
  connect(reply, SIGNAL(readyRead()), this, SLOT(read_get_data()));
  return waitIfSync(reply);
}


QNetworkReply* QWebdav::propfind(const QString & path, const QWebdav::PropNames & props, int depth)
{
  QByteArray query;

  query = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
  query += "<D:propfind xmlns:D=\"DAV:\" >";
  query += "<D:prop>";
  foreach (QString ns, props.keys())
  {
    foreach (const QString key, props[ns]) {
      if (ns == "DAV:")
        query += "<D:" + key + "/>";
      else
        query += "<" + key + " xmlns=\"" + ns + "\"/>";
    }
  }
  query += "</D:prop>";
  query += "</D:propfind>";
  return propfind(path, query, depth);
}


QNetworkReply* QWebdav::propfind(const QString& path, const QByteArray& query, int depth)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));

  QString value;

  if (depth == 2)
    value = "infinity";
  else
    value = QString("%1").arg(depth);
  
  req.setRawHeader(QByteArray("Depth"), value.toUtf8());
  
  return davRequest("PROPFIND", req, query);
}

QNetworkReply* QWebdav::proppatch(const QString& path, const QWebdav::PropValues& props)
{
  QByteArray query;

  query = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
  query += "<D:propertyupdate xmlns:D=\"DAV:\">";
  query += "<D:set>";
  foreach (QString ns, props.keys())
  {
    QMap <QString , QVariant>::const_iterator i;

    for (i = props[ns].constBegin(); i != props[ns].constEnd(); ++i) {
      query += "<D:prop>";
      if (ns == "DAV:") {
        query += "<D:" + i.key() + ">";
        query += i.value().toString();
        query += "</D:" + i.key() + ">" ;
      } else {
        query += "<" + i.key() + " xmlns=\"" + ns + "\">";
        query += i.value().toString();
        query += "</" + i.key() + ">";
      }
      query += "</D:prop>";
    }
  }
  query += "</D:set>";
  query += "</D:propertyupdate>";

  return proppatch(path, query);
}

QNetworkReply* QWebdav::proppatch(const QString & path, const QByteArray & query)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));

  return davRequest("PROPPATCH", req, query);
}

QList<QWebdavUrlInfo> QWebdav::parse(QNetworkReply* reply)
{
  if (!reply->isFinished()) {
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
  }
  
  if (reply->error() != QNetworkReply::NoError) {
    throw std::runtime_error(reply->errorString().toStdString());
  }
  
  QDomDocument doc;
  doc.setContent(reply->readAll(), true);
  QList<QWebdavUrlInfo> result;
  
  for (QDomNode n = doc.documentElement().firstChild(); !n.isNull(); n = n.nextSibling())
  {
    QDomElement element = n.toElement();

    if (element.isNull()) continue;

    QWebdavUrlInfo info(element);
    if (!info.isValid()) continue;

    result << info;
  }
  return result;
}

void QWebdav::int_finished(QNetworkReply* reply)
{
  qDebug() << "reply on:" << reply->property("verb");
  qDebug() << reply->rawHeaderPairs();
}

void QWebdav::read_get_data()
{
  if (QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender())) {
    QObject* ptr = reinterpret_cast<QObject*>(reply->property("iodevice").value<quint64>());
    if (QIODevice* device = qobject_cast<QIODevice*>(ptr)) {
      device->write(reply->read(reply->bytesAvailable()));
    }
  }
}

void QWebdav::authenticate(QNetworkReply*, QAuthenticator* auth)
{
  auth->setUser("kinnalru");
  auth->setPassword("malder22");
}


void QWebdav::setupHeaders(QNetworkRequest& req, quint64 size)
{
//   req.setRawHeader(QByteArray("Host"), host.toUtf8());
  req.setRawHeader(QByteArray("Connection"), QByteArray("Keep-Alive"));
  if (size) {
      req.setHeader(QNetworkRequest::ContentLengthHeader, QVariant(size));
      req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("text/xml; charset=utf-8"));
  }
}

QNetworkReply* QWebdav::davRequest(const QString& verb, QNetworkRequest& req, const QByteArray& data)
{
  QBuffer* buffer = new QBuffer();
  buffer->setData(data);
  buffer->open(QIODevice::ReadOnly);
  QNetworkReply* reply = davRequest(verb, req, buffer); 
  connect(reply, SIGNAL(finished()), buffer, SLOT(deleteLater()));
  return waitIfSync(reply);
}

QNetworkReply* QWebdav::davRequest(const QString& verb, QNetworkRequest& req, QIODevice* data)
{
  qDebug() << "davRequest[" << verb << "]:" << req.url();
  setupHeaders(req, data->size());
  QNetworkReply* reply = sendCustomRequest(req, verb.toUtf8(), data);
  reply->setProperty("verb", verb.toUtf8());
  return waitIfSync(reply);
}

QNetworkReply* QWebdav::mkcol(const QString & dir)
{
  QNetworkRequest req;
  req.setUrl(mkurl(dir));
  
  return davRequest("MKCOL", req);
}

QNetworkReply* QWebdav::mkdir(const QString & dir)
{
  mkdir(dir);
}

QNetworkReply* QWebdav::copy(const QString& oldname, const QString& newname, bool overwrite)
{
  QNetworkRequest req;
  req.setUrl(mkurl(oldname));
  
  req.setRawHeader(QByteArray("Destination"), newname.toUtf8());
  req.setRawHeader(QByteArray("Depth"), QByteArray("infinity"));
  req.setRawHeader(QByteArray("Overwrite"), QByteArray(overwrite ? "T" : "F"));
  return davRequest("COPY", req);
}

QNetworkReply* QWebdav::rename(const QString& oldname, const QString& newname, bool overwrite)
{
  return move(oldname, newname, overwrite);
}

QNetworkReply* QWebdav::move(const QString& oldname, const QString& newname, bool overwrite)
{
  QNetworkRequest req;
  req.setUrl(mkurl(oldname));

  req.setRawHeader(QByteArray("Destination"), newname.toUtf8());
  req.setRawHeader(QByteArray("Depth"), QByteArray("infinity"));
  req.setRawHeader(QByteArray("Overwrite"), QByteArray(overwrite ? "T" : "F"));
  return davRequest("MOVE", req);
}

QNetworkReply* QWebdav::rmdir(const QString& dir)
{
  return remove(dir);
}

QNetworkReply* QWebdav::remove(const QString& path)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));
  return davRequest("DELETE", req);
}

QUrl QWebdav::mkurl(const QString& path) const
{
  QUrl url(url_);
  url.setPath(url_.path() + "/" + path);
  return url;
}

QNetworkReply* QWebdav::waitIfSync(QNetworkReply* reply)
{
  if (type_ == Async) return reply;
  if (reply->isFinished()) return reply;
    
  QEventLoop loop;
  connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
  loop.exec();
  
  if (reply->error() != QNetworkReply::NoError) {
    qDebug() << "Webdav Error:" << reply->errorString();
    qDebug() << reply->rawHeaderPairs();
    qDebug() << reply->readAll();
    throw std::runtime_error(reply->errorString().toStdString().c_str());
  }
  
  return reply;
}



