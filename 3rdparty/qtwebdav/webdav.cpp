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
#include <QBuffer>
#include <QStringList>
#include <QEventLoop>

#include <stdexcept>

#include "webdav.h"
#include "webdav_url_info.h"


QWebdav::QWebdav(const QUrl& url, QObject* parent)
  : QNetworkAccessManager(parent)
  , url_(url)
{
  qDebug() << "QWebdav: " << url;
  connect(this, SIGNAL(finished(QNetworkReply*)), this, SLOT(int_finished(QNetworkReply*)));
}

QWebdav::~QWebdav ()
{
}

QNetworkReply* QWebdav::setPermissions(const QString& path, QFile::Permissions perms)
{
  PropValues values;
  QMap <QString, QVariant> data;
  data["permissions"] = static_cast<quint32>(perms);
  values["DAVQT:"] = data;
  return proppatch(path, values);
  
}


QNetworkReply* QWebdav::list(const QString & dir)
{
  QWebdav::PropNames query;
  QStringList props;

  props << "creationdate";
  props << "getcontentlength";
  props << "displayname";
  props << "source";
  props << "getcontentlanguage";
  props << "getcontenttype";
  props << "executable";
  props << "getlastmodified";
  props << "getetag";
  props << "resourcetype";

  query["DAV:"] = props;
  query[""] = props;
  query["DAVQT:"] = QStringList() << "permissions";
  query[""] = QStringList() << "permissions";
  
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
  QBuffer* buffer = new QBuffer(&data);
  return put(path, buffer);
}

QNetworkReply* QWebdav::put(const QString& path, QIODevice * data)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));

  return davRequest("PUT", req, data);
}

void QWebdav::get(const QString& path, QIODevice* data)
{
  QNetworkRequest req;
  req.setUrl(mkurl(path));
  
  QNetworkReply* reply = QNetworkAccessManager::get(req);
  if (!reply->isFinished()) {
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
  }
  
  if (reply->error() != QNetworkReply::NoError) {
    throw std::runtime_error(reply->errorString().toStdString());
  }
  
  data->write(reply->readAll());
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
  query += "<D:proppatch xmlns:D=\"DAV:\" >";
  query += "<D:prop>";
  foreach (QString ns, props.keys())
  {
    QMap <QString , QVariant>::const_iterator i;

    for (i = props[ns].constBegin(); i != props[ns].constEnd(); ++i) {
      if (ns == "DAV:") {
        query += "<D:" + i.key() + ">";
        query += i.value().toString();
        query += "</D:" + i.key() + ">" ;
      } else {
        query += "<" + i.key() + " xmlns=\"" + ns + "\">";
        query += i.value().toString();
        query += "</" + i.key() + " xmlns=\"" + ns + "\"/>";
      }
    }
  }
  query += "</D:prop>";
  query += "</D:propfind>";

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


void QWebdav::emitListInfos()
{
  QDomDocument multiResponse;
  bool hasResponse = false;

  multiResponse.setContent(buffer, true);

  for ( QDomNode n = multiResponse.documentElement().firstChild();
        !n.isNull(); n = n.nextSibling())
    {
      QDomElement thisResponse = n.toElement();

      if (thisResponse.isNull()) continue;

      
      QWebdavUrlInfo info(thisResponse);
      qDebug() << "info:";
      qDebug() << info.displayName();
      qDebug() << info.source();
      qDebug() << info.properties();

      

      if (!info.isValid()) continue;

      hasResponse = true;
      emit listInfo(info);
    }
}

void QWebdav::int_finished(QNetworkReply* reply)
{
  qDebug() << "reply";
  if (reply->property("verb") == "PROPFIND") {
//     qDebug() << parse(reply);
  }
}

void
QWebdav::setupHeaders(QNetworkRequest& req, quint64 size)
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
  return reply;
}

QNetworkReply* QWebdav::davRequest(const QString& verb, QNetworkRequest& req, QIODevice* data)
{
  qDebug() << "davRequest:" << req.url();
  setupHeaders(req, data->size());
  QNetworkReply* reply = sendCustomRequest(req, verb.toUtf8(), data);
  reply->setProperty("verb", verb.toUtf8());
  return reply;
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

