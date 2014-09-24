/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2013  <copyright holder> <email>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <libgen.h>
#include <sys/stat.h> 
#include <fcntl.h> 

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <neon/ne_alloc.h>
#include <neon/ne_auth.h>
#include <neon/ne_basic.h>
#include <neon/ne_dates.h>
#include <neon/ne_locks.h>
#include <neon/ne_props.h>
#include <neon/ne_request.h>
#include <neon/ne_session.h>
#include <neon/ne_socket.h>
#include <neon/ne_string.h>
#include <neon/ne_uri.h>
#include <neon/ne_utils.h>
#include <neon/ne_xml.h>

#include <QDebug>

#include "tools.h"
#include "session.h"

#define EUNKNOWN 99999

enum {
    ETAG = 0,
    LENGTH,
    CREATION,
    MODIFIED,
    TYPE,
    EXECUTE,
    PERMISSIONS,
    END
};

static const std::vector<ne_propname> prop_names = [] {
    std::vector<ne_propname> v(END + 1);
    v[ETAG]     = {"DAV:", "getetag"};
    v[LENGTH]   = {"DAV:", "getcontentlength"};
    v[CREATION] = {"DAV:", "creationdate"};
    v[MODIFIED] = {"DAV:", "getlastmodified"};
    v[TYPE]     = {"DAV:", "resourcetype"};
    v[EXECUTE]  = {"http://apache.org/dav/props/", "executable"};
    v[PERMISSIONS]  = {"DAVQT:", "permissions"};
    v[END]      = {NULL, NULL}; 
    return v;
} ();

static const std::vector<ne_propname> anonymous_prop_names = [] {
    std::vector<ne_propname> v = prop_names;
    for(auto it = v.begin(); it != v.end(); ++ it) it->nspace = NULL;
    return v;
} ();

typedef std::function<void (ne_session_status, const ne_session_status_info*)> Notify;

struct auth_data_t {
    QString user;
    QString passwd;
};

struct neon_context_t {
//     QString etag;
    stat_t stat;
    volatile bool& cancell;
    ne_session* session;
};

struct cache_context_t {
    QString path;
    QList<remote_res_t> resources;
};

struct session_t::Pimpl {
    QString scheme;
    auth_data_t auth;
    volatile bool cancell;
    Notify notify;    
    bool notify_enabled;
    std::shared_ptr<ne_session> session;
    QDateTime lastprogress;
};

static int auth(void *userdata, const char *realm, int attempt, char *user, char *pwd)
{
    if (attempt) {
        return attempt;
    }

    auth_data_t* data = reinterpret_cast<auth_data_t*>(userdata);
    
    strncpy(user, qPrintable(data->user), NE_ABUFSIZ - 1);
    strncpy(pwd, qPrintable(data->passwd), NE_ABUFSIZ - 1);



    return 0;
}

static int ssl_verify(void *userdata, int failures, const ne_ssl_certificate *cert)
{
    return 0;
    
    char *issuer = ne_ssl_readable_dname(ne_ssl_cert_issuer(cert));
    char *subject = ne_ssl_readable_dname(ne_ssl_cert_subject(cert));
    char *digest = (char*)ne_calloc(NE_SSL_DIGESTLEN);
    if (!issuer || !subject || ne_ssl_cert_digest(cert, digest) != 0) {
        std::cerr << "error processing server certificate" << std::endl;
            
        if (issuer) free(issuer);
        if (subject) free(subject);
        if (digest) free(digest);
        return -1;
    }

    int ret = -1;
    
    if (failures & NE_SSL_NOTYETVALID)
        std::cerr << "the server certificate is not yet valid" << std::endl;
    if (failures & NE_SSL_EXPIRED)
        std::cerr << "the server certificate has expired" << std::endl;
    if (failures & NE_SSL_IDMISMATCH)
        std::cerr << "the server certificate does not match the server name" << std::endl;
    if (failures & NE_SSL_UNTRUSTED)
        std::cerr << "the server certificate is not trusted" << std::endl;
    if (failures & ~NE_SSL_FAILMASK)
        std::cerr << "unknown certificate error" << std::endl;

    printf(("  issuer:      %s"), issuer);
    printf("\n");
    printf(("  subject:     %s"), subject);
    printf("\n");
    printf(("  identity:    %s"), ne_ssl_cert_identity(cert));
    printf("\n");//     ne_session* session = ne_session_create("https", "webdav.yandex.ru", ne_uri_defaultport("https"));
    printf(("  fingerprint: %s"), digest);
    printf("\n");
        printf(("You only should accept this certificate, if you can\n"
                    "verify the fingerprint! The server might be faked\n"
                    "or there might be a man-in-the-middle-attack.\n"));
        printf(("Accept certificate for this session? [y,N] "));
        char *s = NULL;
        size_t n = 0;
        ssize_t len = 0;
        len = getline(&s, &n, stdin);
        if (len < 0)
            abort();
        if (rpmatch(s) > 0)
            ret = 0;
        free(s);


    if (failures & NE_SSL_NOTYETVALID)
        std::cerr << "the server certificate is not yet valid" << std::endl;
    if (failures & NE_SSL_EXPIRED)
        std::cerr << "the server certificate has expired" << std::endl;
    if (failures & NE_SSL_IDMISMATCH)
        std::cerr << "the server certificate does not match the server name" << std::endl;
    if (failures & NE_SSL_UNTRUSTED)
        std::cerr << "the server certificate is not trusted" << std::endl;
    if (failures & ~NE_SSL_FAILMASK)
        std::cerr << "unknown certificate error" << std::endl;
    
    ne_ssl_cert_identity(cert);
    

    if (issuer) free(issuer);
    if (subject) free(subject);
    if (digest) free(digest);
    return ret;
}

inline QString normalize_etag(const char *etag)
{
    if (!etag) return QString();

    const char * e = etag;
    if (*e == 'W') return QString();
    if (!*e) return QString();

    return QString(etag);
}

void create_request_handler(ne_request *req, void *userdata, const char *method, const char *requri) {
    neon_context_t* ctx = reinterpret_cast<neon_context_t*>(userdata);
    
//     if (ctx->etag.isNull()) {
//         ne_add_request_header(req, "If-None-Match", "*");
//     }
//     else if(!ctx->etag.isEmpty()) {
//         ne_add_request_header(req, "If-Match", qPrintable(ctx->etag));
//     }
//     else
//         Q_ASSERT(ctx->etag == "");
}

int post_send_handler(ne_request *req, void *userdata, const ne_status *status) {
    
    neon_context_t* ctx = reinterpret_cast<neon_context_t*>(userdata);
    
    const char *value = ne_get_response_header(req, "Last-Modified");
    if (!value) value = ne_get_response_header(req, "Date");

    if (value) {
        ctx->stat.mtime = ne_httpdate_parse(value);
    } else {
        ctx->stat.mtime = 0;
    }
    
    return NE_OK;
}

struct hook_helper_t {
    ne_session* session;
    neon_context_t* ctx;
    
    hook_helper_t(ne_session* s, neon_context_t* c) : session(s), ctx(c) {
        Q_ASSERT(session);
        Q_ASSERT(ctx);
        ne_hook_create_request(session, create_request_handler, ctx);
        ne_hook_post_send(session, post_send_handler, ctx);
    }
    
    ~hook_helper_t() {
        ne_unhook_post_send(session, post_send_handler, ctx);        
        ne_unhook_create_request(session, create_request_handler, ctx);
    }
};

void cache_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set)
{
    cache_context_t* ctx = reinterpret_cast<cache_context_t*>(userdata);

    assert(ctx);
    assert(uri);
    assert(set);
    
    remote_res_t resource;
    
    resource.path = QString::fromUtf8(ne_path_unescape(uri->path));
    
    const char *data;

    data = ne_propset_value(set, &prop_names[ETAG]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[ETAG]);
    if (data) resource.etag = data;
    
    data = ne_propset_value(set, &prop_names[TYPE]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[TYPE]);
    resource.dir = (data && strstr(data, "collection"));

    data = ne_propset_value(set, &prop_names[LENGTH]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[LENGTH]);
    if (data) resource.size = boost::lexical_cast<off_t>(data);

    data = ne_propset_value(set, &prop_names[PERMISSIONS]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[PERMISSIONS]);
    if (data) {
        resource.perms = QFile::Permissions(boost::lexical_cast<int>(data));
    }
    
    data = ne_propset_value(set, &prop_names[CREATION]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[CREATION]);
    resource.ctime = 0;
    if (data) {
        resource.ctime = ne_iso8601_parse(data);
        if (resource.ctime == static_cast<time_t>(-1))
            resource.ctime = ne_httpdate_parse(data);
        if (resource.ctime == static_cast<time_t>(-1))
            resource.ctime = 0;
    }
    
    data = ne_propset_value(set, &prop_names[MODIFIED]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[MODIFIED]);
    resource.mtime = 0;
    if (data) {
        resource.mtime = ne_httpdate_parse(data);
        if (resource.mtime == static_cast<time_t>(-1))
            resource.mtime = ne_iso8601_parse(data);
        if (resource.mtime == static_cast<time_t>(-1))
            resource.mtime = 0;
    }
    
    data = ne_propset_value(set, &prop_names[EXECUTE]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[EXECUTE]);
    resource.exec = remote_res_t::none;
    if (!data) {
        resource.exec = (resource.dir) ? remote_res_t::none : remote_res_t::not_executable;
    } else if (*data == 'T') {
        resource.exec = remote_res_t::executable;
    }
    
    if (resource.path.right(1) == "/") resource.path = resource.path.left(resource.path.length() - 1);
    
    if (QFileInfo(ctx->path).absoluteFilePath() == QFileInfo(resource.path).absoluteFilePath()) {
        resource.name = ".";        
    }
    else {
        resource.name = QFileInfo(resource.path).fileName();        
    }
    
    qDebug() << "resource uri:" << ne_path_unescape(uri->path);
    qDebug() << "resource ctx:" << ctx->path;
    qDebug() << "resource ctx a:" << QFileInfo(ctx->path).absoluteFilePath();
    qDebug() << "resource path:" << resource.path ;
    qDebug() << "resource path a:" << QFileInfo(resource.path).absoluteFilePath() ;
    qDebug() << "resource name:" << resource.name ;
    
    ctx->resources << resource;
}

void notify(void *userdata, ne_session_status status, const ne_session_status_info *info)
{
    Notify* fn = reinterpret_cast<Notify*>(userdata);
    fn->operator()(status, info);
}

static int http_code(ne_session* session) {
    const char *p = ne_get_error(session);
    char *q;
    int err;

    err = strtol(p, &q, 10);
    if (p == q) {
        err = EUNKNOWN;
    }
    return err;
}

session_t::session_t(QObject* parent, const QString& schema, const QString& host, quint32 port)
    : QObject(parent)
    , p_(new Pimpl)
{
    p_->scheme = schema;
    p_->lastprogress = QDateTime::currentDateTime();
    p_->cancell = false;
    p_->session.reset(
        ne_session_create(schema.toLocal8Bit().constData(), host.toLocal8Bit().constData(), (port == -1) ? ne_uri_defaultport("https") : port),
        ne_session_destroy
    );
    
    if (!p_->session.get()) throw std::runtime_error("Can't create session");
    
    
    p_->notify_enabled = false;
    p_->notify = [this] (ne_session_status status, const ne_session_status_info *info) {
        if (p_->cancell) {
            ne_set_notifier(p_->session.get(), NULL, NULL);
            ne_close_connection(p_->session.get());
        }
        
        if (!p_->notify_enabled) return;
        
        switch (status) {
            case ne_status_lookup:
            case ne_status_connecting:
                break;
            case ne_status_connected:
                Q_EMIT connected();
                break;
            case ne_status_sending: 
                if (info->sr.progress == info->sr.total) {
                    p_->lastprogress = QDateTime::currentDateTime();
                    Q_EMIT put_progress(info->sr.progress, info->sr.total);
                }
                else if (p_->lastprogress.msecsTo(QDateTime::currentDateTime()) > 300) {
                    p_->lastprogress = QDateTime::currentDateTime();
                    Q_EMIT put_progress(info->sr.progress, info->sr.total);
                }
                break;
            case ne_status_recving:
                if (info->sr.progress == info->sr.total) {
                    p_->lastprogress = QDateTime::currentDateTime();
                    Q_EMIT get_progress(info->sr.progress, info->sr.total);
                }
                else if (p_->lastprogress.msecsTo(QDateTime::currentDateTime()) > 300) {
                    p_->lastprogress = QDateTime::currentDateTime();
                    Q_EMIT get_progress(info->sr.progress, info->sr.total);
                }
                break;
            case ne_status_disconnected:
                Q_EMIT disconnected();
                break;
            default: 
                Q_ASSERT(!"Unhandled status type");
        };
    }; 
    
    ne_set_notifier(p_->session.get(), notify, &p_->notify);
}

session_t::~session_t()
{
    ne_set_notifier(p_->session.get(), NULL, NULL);
}

void session_t::set_auth(const QString& user, const QString& password)
{
    p_->auth.user = user;
    p_->auth.passwd = password;
    ne_add_server_auth(p_->session.get(), NE_AUTH_ALL, auth, &p_->auth);
}

void session_t::set_ssl()
{
    if (p_->scheme != "https") return;
    
    ne_ssl_set_verify(p_->session.get(), ssl_verify, NULL);
    ne_ssl_trust_default_ca(p_->session.get());
}

void session_t::open()
{
    std::shared_ptr<char> path(ne_path_escape("/"), free);    

    ne_server_capabilities caps = {0, 0, 0};
    int ret = ne_options(p_->session.get(), path.get(), &caps);
    if (ret) {
        throw ne_exception_t(http_code(p_->session.get()), QString("Can't get server options:") + ne_get_error(p_->session.get()));
    }
}

void session_t::cancell()
{
    p_->cancell = true;
}

bool session_t::is_closed() const
{
    return p_->cancell;
}


QList<remote_res_t> session_t::get_resources(QString unescaped_path) {
    unescaped_path.prepend("/").replace("///", "/").replace("//", "/"); 
    
    qDebug() << Q_FUNC_INFO << "rawpath: " << unescaped_path;
    
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);  

    qDebug() << Q_FUNC_INFO << "path: " << path.get();
    

    std::shared_ptr<ne_propfind_handler> ph(
        ne_propfind_create(p_->session.get(), path.get(), NE_DEPTH_ONE),
        ne_propfind_destroy            
    );

    cache_context_t ctx;
    ctx.path = unescaped_path;  

    if (int err = ne_propfind_named(ph.get(), &prop_names[0], cache_result, &ctx)) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }
    return ctx.resources;
}



stat_t session_t::head(const QString& unescaped_path)
{
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);
    
    std::shared_ptr<ne_request> request(
        ne_request_create(p_->session.get(), "HEAD", path.get()),
        ne_request_destroy
    );
    
    if (ne_request_dispatch(request.get()) != NE_OK) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }

    stat_t stat;
    
    const char *value = ne_get_response_header(request.get(), "Last-Modified");
    if (!value) value = ne_get_response_header(request.get(), "Date");
    
    if (value) {
        stat.mtime = ne_httpdate_parse(value);
    }
    
    value = ne_get_response_header(request.get(), "Content-Length");
    
    if (value) {
        stat.size = strtol(value, NULL, 10);
    }
    
    return stat;
}



stat_t session_t::set_permissions(const QString& unescaped_path, QFile::Permissions perms)
{
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);    
    const std::string value = boost::lexical_cast<std::string>(perms);
    
    const ne_proppatch_operation ops[2] = {
        {
            &prop_names[PERMISSIONS],
            ne_propset,
            value.c_str()
        },
        NULL
    };
    
    neon_context_t ctx{
        stat_t(),
        p_->cancell,
        p_->session.get()
    };  

    hook_helper_t hooks(p_->session.get(), &ctx);
    if (ne_proppatch(p_->session.get(), path.get(), ops) != NE_OK) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }

    return ctx.stat;
}

stat_t session_t::get(const QString& unescaped_path, int fd)
{
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);
    neon_context_t ctx{
        stat_t(),
        p_->cancell,
        p_->session.get()
    };  
    
    scoped_value<bool> v(p_->notify_enabled, true, false);
    hook_helper_t hooker(p_->session.get(), &ctx);
    if (ne_get(p_->session.get(), path.get(), fd) != NE_OK) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }

    return ctx.stat;
}

stat_t session_t::put(const QString& unescaped_path, int fd)
{
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);
    neon_context_t ctx{
        stat_t(),
        p_->cancell,
        p_->session.get()
    };  
    
    scoped_value<bool> v(p_->notify_enabled, true, false);
    hook_helper_t hooker(p_->session.get(), &ctx);
    if (ne_put(p_->session.get(), path.get(), fd) != NE_OK) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }
    
    return ctx.stat;
}

void session_t::remove(const QString& unescaped_path)
{
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);
    neon_context_t ctx{
        stat_t(),
        p_->cancell,
        p_->session.get()
    }; 
    
    hook_helper_t hooker(p_->session.get(), &ctx);
    if (ne_delete(p_->session.get(), path.get()) != NE_OK) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }
}

stat_t session_t::mkcol(const QString& raw)
{
    QString unescaped_path = raw;
    unescaped_path.append("/").replace("//", "/");
    
    std::shared_ptr<char> path(ne_path_escape(qPrintable(unescaped_path)), free);
    
    neon_context_t ctx{
        stat_t(),
        p_->cancell,
        p_->session.get()
    };
    
    hook_helper_t hooker(p_->session.get(), &ctx);
    if (ne_mkcol(p_->session.get(), path.get()) != NE_OK) {
        throw ne_exception_t(http_code(p_->session.get()), ne_get_error(p_->session.get()));
    }
    
    return ctx.stat;
}

