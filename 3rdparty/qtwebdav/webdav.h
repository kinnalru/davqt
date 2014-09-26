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

#ifndef QWEBDAV_H
#define QWEBDAV_H

#include <QNetworkAccessManager>
#include <QUrlInfo>
#include <QNetworkReply>
#include <QUrl>
#include <QDateTime>
#include <QFile>
#include <QDomNodeList>
#include <QMap>
#include <QEventLoop>

class QWebdavUrlInfo;

/**
 * @brief Main class used to handle the webdav protocol
 */
class QWebdav : public QNetworkAccessManager
{
  Q_OBJECT
public:
  QWebdav(const QUrl& url, QObject* parent = 0);
  ~QWebdav();

  typedef QMap <QString, QMap <QString, QVariant>> PropValues;
  typedef QMap <QString, QStringList> PropNames;

  QNetworkReply* setPermissions(const QString& path, QFile::Permissions perms);
  
  QNetworkReply* list(const QString& dir);
  QNetworkReply* search(const QString& path, const QString& query);
  QNetworkReply* put(const QString& path, QByteArray& data);
  QNetworkReply* put(const QString& path, QIODevice* data);
    QNetworkReply* get(const QString& path, QIODevice* device);

  QNetworkReply* mkcol(const QString & dir);

  QNetworkReply* mkdir( const QString & dir);
  QNetworkReply* copy ( const QString & oldname, const QString & newname,
	     bool overwrite = false );
  QNetworkReply* rename ( const QString & oldname, const QString & newname,
	       bool overwrite = false );
  QNetworkReply* move ( const QString & oldname, const QString & newname,
	     bool overwrite = false );
  QNetworkReply* rmdir ( const QString & dir );
  QNetworkReply* remove (const QString & path);

  QNetworkReply* propfind ( const QString & path, const QByteArray & query, int depth = 0 );
  QNetworkReply* propfind ( const QString & path, const QWebdav::PropNames & props,
		 int depth = 0 );

  QNetworkReply* proppatch ( const QString & path, const QWebdav::PropValues & props);
  QNetworkReply* proppatch ( const QString & path, const QByteArray & query );

  int setHost ( const QString &, quint16 );

  QList<QWebdavUrlInfo> parse(QNetworkReply* reply);
  
 private slots:
  void int_finished(QNetworkReply* resp);
  void read_get_data();

 private:
  void emitListInfos();
  void davParsePropstats( const QDomNodeList & propstat );
  int codeFromResponse( const QString& response );
  QDateTime parseDateTime( const QString& input, const QString& type );
  QNetworkReply* davRequest(const QString & reqVerb,
                 QNetworkRequest & req,
                 const QByteArray & data = QByteArray());
  QNetworkReply* davRequest(const QString& verb, QNetworkRequest& req, QIODevice* data);
  void setupHeaders(QNetworkRequest & req, quint64 size);
  
  QUrl mkurl(const QString& path) const;

 private:
  Q_DISABLE_COPY(QWebdav);
  const QUrl url_;
};

class QReplyWaiter : public QObject{
  Q_OBJECT
public:
  QReplyWaiter(QNetworkReply* reply, bool wait = false) {
    connect(reply, SIGNAL(finished()), SLOT(finished()));
    if (wait) {loop_.exec();}
  }
  
  virtual ~QReplyWaiter() {}
  
  int wait() { return loop_.exec(); }
  
  QNetworkReply* reply() const {return reply_;}
  
private Q_SLOTS:
  void finished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    
    if (reply->error() != QNetworkReply::NoError) {
      loop_.exit(-1);
    } else {
      loop_.exit(0);
    }
  }
private:
  QNetworkReply* reply_;
  QEventLoop loop_;
};


#endif // QWEBDAV_H
