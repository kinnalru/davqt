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

#include <QUrl>
#include <QDebug>

#include "webdav_url_info.h"

QWebdavUrlInfo::QWebdavUrlInfo()
{
}

QWebdavUrlInfo::~QWebdavUrlInfo()
{
}

QWebdavUrlInfo::QWebdavUrlInfo(const QDomElement & dom)
{
  QDomElement href = dom.namedItem( "href" ).toElement();

  if (!href.isNull())
  {
    QString urlStr = QUrl::fromPercentEncoding(href.text().toUtf8());
    QDomNodeList propstats = dom.elementsByTagName("propstat");
    davParsePropstats(urlStr, propstats, dom.cloneNode());
  }
}

QWebdavUrlInfo::QWebdavUrlInfo (const QWebdavUrlInfo & wui)
  : QUrlInfo(wui)
  , properties_(wui.properties_)
  , createdAt_(wui.createdAt_)
  , displayName_(wui.displayName_)
  , source_(wui.source_)
  , contentLanguage_(wui.contentLanguage_)
  , entityTag_(wui.entityTag_)
  , mimeType_(wui.mimeType_)
{
}

int QWebdavUrlInfo::codeFromResponse(const QString& response)
{
  int firstSpace = response.indexOf(' ');
  int secondSpace = response.indexOf(' ', firstSpace + 1);
  return response.mid(firstSpace + 1, secondSpace - firstSpace - 1).toInt();
}

QDateTime QWebdavUrlInfo::parseDateTime(const QString& input, const QString& type)
{
  QDateTime datetime;

  if (type == "dateTime.tz")
    datetime =  QDateTime::fromString( input, Qt::ISODate );
  else if (type == "dateTime.rfc1123")
    datetime = QDateTime::fromString(input);

  if (!datetime.isNull()) return datetime;

  datetime = QDateTime::fromString(input.left(19), "yyyy-MM-dd'T'hh:mm:ss");
  if (!datetime.isNull()) return datetime;
  
  datetime = QDateTime::fromString(input.mid(5, 20) , "d MMM yyyy hh:mm:ss");
  if (!datetime.isNull()) return datetime;
  
  QDate date;
  QTime time;

  date = QDate::fromString(input.mid(5, 11) , "d MMM yyyy");
  time = QTime::fromString(input.mid(17, 8) , "hh:mm:ss");
  return QDateTime(date, time);
}

void QWebdavUrlInfo::davParsePropstats(const QString& path, const QDomNodeList& propstats, const QDomNode& node)
{
  bool dir_initialized = false;
  bool exe_initialized = false;
  bool executable = false;

  setName(path);

  for (int i = 0; i < propstats.count(); i++) {
    QDomElement propstat = propstats.item(i).toElement();
    QDomElement status = propstat.namedItem( "status" ).toElement();

    if (status.isNull()) {
      qDebug() << "Error, no status code in this propstat";
      return;
    }

    if (codeFromResponse(status.text()) == 404) continue;

    QDomElement prop = propstat.namedItem( "prop" ).toElement();

    if (prop.isNull()) {
      qDebug() << "Error: no prop segment in this propstat.";
      return;
    }

    for (QDomNode n = prop.firstChild(); !n.isNull(); n = n.nextSibling()) {
      QDomElement property = n.toElement();

      if (property.isNull()) continue;

      properties_[property.namespaceURI()][property.tagName()] = property.text();

      if (property.tagName() == "creationdate") {
        if (createdAt().isNull()) {
          setCreatedAt(parseDateTime(property.text(), property.attribute("dt")));
        }
      }
      else if (property.tagName() == "permissions") {
        if (permissions() == 0) {
          setPermissions(property.text().toUInt());
        }
      }
      else if (property.tagName() == "getcontentlength") {
        qDebug() << "First size:" << size();
        if (size() == -1) {
          setSize(property.text().toULong());
        }
      }
      else if( property.tagName() == "displayname") {
        if (displayName().isNull()) {
          setDisplayName(property.text());
        }
      }
      else if (property.tagName() == "source") {
        QDomElement source;

        source = property.namedItem( "link" ).toElement()
          .namedItem( "dst" ).toElement();

        if (!source.isNull()) {
          if (this->source().isNull()) {
            setSource(source.text());
          }
        }
      }
      else if (property.tagName() == "getcontentlanguage") {
        if (contentLanguage().isNull()) {
          setContentLanguage(property.text());
        }
      }
      else if (property.tagName() == "getcontenttype") {
        if (property.text() == "httpd/unix-directory") {
          if (!dir_initialized) {
            dir_initialized = true;
            setDir(true);
          }
        }
        
        if (this->mimeType().isNull()) {
          setMimeType(property.text());
        }
      }
      else if (property.tagName() == "executable")
      {
        if (property.text() == "T") {
          if (!exe_initialized) {
            executable = true;
          }
        }
      }
      else if (property.tagName() == "getlastmodified") {
        if (lastModified().isNull()) {
          setLastModified(parseDateTime( property.text(), property.attribute("dt") ));
        }
      }
      else if (property.tagName() == "getetag") {
        if (entityTag().isNull()) {
          setEntitytag(property.text());
        }
      }
      else if (property.tagName() == "resourcetype") {
        if (!property.namedItem("collection").toElement().isNull()) {
          setDir(true);
        }
      }
      else {
        qDebug() << "Found unknown webdav property: " << property.tagName() << property.text();
      }
    }
    
  } 
  
  setFile(!isDir());

  if (isDir() && !name().endsWith("/"))
    setName(name() + "/");

  if (permissions() == 0) {
    setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (isDir()) {
      QFile::Permissions perms(permissions());
      setPermissions(perms | QFile::ExeUser | QFile::ExeOwner);
    }
  }
  
  
  if (exe_initialized && executable) {
    QFile::Permissions perms(permissions());
    setPermissions(perms | QFile::ExeUser | QFile::ExeOwner);
  }
  if (isDir()) {
    setMimeType(QString());
  }

}


void QWebdavUrlInfo::setCreatedAt(const QDateTime & date)
{
  createdAt_ = date;
}

void QWebdavUrlInfo::setDisplayName(const QString & name)
{
  displayName_ = name;
}

void QWebdavUrlInfo::setSource(const QString & source)
{
  source_ = source;
}

void QWebdavUrlInfo::setContentLanguage(const QString & lang)
{
  contentLanguage_ = lang;
}

void QWebdavUrlInfo::setEntitytag(const QString & etag)
{
  entityTag_ = etag;
}

void QWebdavUrlInfo::setMimeType(const QString & mime)
{
  mimeType_ = mime;
}

QDateTime QWebdavUrlInfo::createdAt() const
{
  return createdAt_;
}

QString QWebdavUrlInfo::displayName() const
{
  return displayName_;
}

QString QWebdavUrlInfo::source() const
{
  return source_;
}

QString QWebdavUrlInfo::contentLanguage() const
{
  return contentLanguage_;
}

QString QWebdavUrlInfo::entityTag() const
{
  return entityTag_;
}

QString QWebdavUrlInfo::mimeType() const
{
  return mimeType_;
}

const QWebdav::PropValues& QWebdavUrlInfo::properties() const
{
  return properties_;
}

