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
#include <QDate>

#include "session.h"


enum {
    ETAG = 0,
    LENGTH,
    CREATION,
    MODIFIED,
    TYPE,
    EXECUTE,
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
    v[END]      = {NULL, NULL}; 
    return v;
} ();

static const std::vector<ne_propname> anonymous_prop_names = [] {
    std::vector<ne_propname> v = prop_names;
    BOOST_FOREACH(auto& pn, v) pn.nspace = NULL;
    return v;
} ();

struct request_ctx_t {
    std::string path;
    std::vector<resource_t> resources;
};

void cache_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set)
{
    request_ctx_t* ctx = reinterpret_cast<request_ctx_t*>(userdata);

    std::cerr << "cr0" << std::endl;
    
    assert(ctx);
    assert(uri);
    assert(set);
    
    //FIXME check
    //if (!res || !uri || !uri->path || !set) return;

    resource_t resource;
    
    resource.path = ne_path_unescape(uri->path);
    
    const char *data;

    data = ne_propset_value(set, &prop_names[ETAG]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[ETAG]);
    if (data) resource.etag = data;
    
    data = ne_propset_value(set, &prop_names[TYPE]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[TYPE]);
    resource.dir = (data && strstr(data, "collection"));

    data = ne_propset_value(set, &prop_names[LENGTH]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[LENGTH]);
    resource.size = boost::lexical_cast<off_t>(data);

    data = ne_propset_value(set, &prop_names[CREATION]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[CREATION]);
    resource.ctime = 0;
    if (data) {
        resource.ctime = ne_iso8601_parse(data);
        if (resource.ctime == (time_t) -1)
            resource.ctime = ne_httpdate_parse(data);
        if (resource.ctime == (time_t) -1)
            resource.ctime = 0;
    }
    
    data = ne_propset_value(set, &prop_names[MODIFIED]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[MODIFIED]);
    resource.mtime = 0;
    if (data) {
        resource.mtime = ne_httpdate_parse(data);
        if (resource.mtime == (time_t) -1)
            resource.mtime = ne_iso8601_parse(data);
        if (resource.mtime == (time_t) -1)
            resource.mtime = 0;
    }
    
    data = ne_propset_value(set, &prop_names[EXECUTE]);
    if (!data) data = ne_propset_value(set, &anonymous_prop_names[EXECUTE]);
    resource.exec = none;
    if (!data) {
        resource.exec = (resource.dir) ? none : no_exec;
    } else if (*data == 'T') {
        resource.exec = exec;
    }
    
    std::shared_ptr<char> path(strdup(resource.path.c_str()), free);
    resource.name = basename(path.get());
    
    std::shared_ptr<char> qpath(strdup(ctx->path.c_str()), free);
    
    if (resource.name == std::string(basename(qpath.get()))) 
        resource.name = ".";
    
    ctx->resources.push_back(resource);
    return;
}

static int content_reader(void *userdata, const char *buf, size_t len)
{
    session_t::ContentHandler* handler = reinterpret_cast<session_t::ContentHandler*>(userdata);
    assert(handler);
    assert(buf);

    try {
        const size_t handled = (*handler)(buf, len);
        if (len != handled) {
            std::cerr << "content handler handles " << handled << " bytes instead of " << len << " in buffer\n";
            return NE_ERROR;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "content handler throw exception:" << e.what() << "\n";
        return NE_ERROR;
    }

    return NE_OK;
}

struct session_t::Pimpl {
    
    std::shared_ptr<ne_session> session;
};

session_t::session_t(const std::string& schema, const std::string& host, unsigned int port)
    : p_(new Pimpl)
{
    p_->session.reset(
        ne_session_create(schema.c_str(), host.c_str(), (port == -1) ? ne_uri_defaultport("https") : port),
        ne_session_destroy
    );
    
    if (!p_->session.get()) throw std::runtime_error("Can't create session");
}

session_t::~session_t()
{}

ne_session* session_t::session() const
{
    return p_->session.get();
}

std::vector<resource_t> session_t::get_resources(const std::string& path) {
    
    std::shared_ptr<ne_propfind_handler> ph(
        ne_propfind_create(p_->session.get(), path.c_str(), NE_DEPTH_ONE),
        ne_propfind_destroy            
    );

    request_ctx_t ctx;
    ctx.path = path;

    if (int err = ne_propfind_named(ph.get(), &prop_names[0], cache_result, &ctx)) {
        std::cerr << "Err:" << err << " error:" << errno << std::endl;
        throw std::runtime_error(ne_get_error(p_->session.get()));
    }
    
    return ctx.resources;
}

std::vector<std::string> session_t::ls(const std::string& path)
{
    std::vector<std::string> res;
    
    BOOST_FOREACH(const auto& r, get_resources(path)) {
        res.push_back(r.name);
    }

    return res;
}

time_t session_t::mtime(const std::string& path)
{
    BOOST_FOREACH(const auto& r, get_resources(path)) {
        return r.mtime;
    }
    throw std::runtime_error("error in mtime");
}

std::string normalize_etag(const char *etag)
{
    if (!etag) return std::string();

    const char * e = etag;
    if (*e == 'W') return std::string();
    if (!*e) return std::string();

    return std::string(etag);
}

int post_send_handler(ne_request *req, void *userdata, const ne_status *status) {
    
    stat_t* data = reinterpret_cast<stat_t*>(userdata);
    
    data->etag = normalize_etag(ne_get_response_header(req, "ETag"));
    
    const char *value = ne_get_response_header(req, "Last-Modified");
    if (!value) value = ne_get_response_header(req, "Date");
    
    if (value) {
        data->remote_mtime = ne_httpdate_parse(value);
    } else {
        data->remote_mtime = 0;
    }
    
    std::cerr << "status:" << status->code << std::endl;
    std::cerr << "status2:" << status->reason_phrase << std::endl;
    
    return NE_OK;
}

void pre_send_handler(ne_request *req, void *userdata, ne_buffer *header) {
// void pre_send_handler(ne_request *req, void *userdata, const char *method, const char *requri) {
//     ne_add_request_header(req, "If-None-Match", "*");
//     ne_add_request_header(req, "If-None-Match", "*");
    ne_add_request_header(req, "If-None-Match", "*");
    ne_add_request_header(req, "X-Len-Test", "123");
//     ne_buffer_zappend(header, "If-None-Match: *");
//     ne_buffer_zappend(header, "If-None-Match: *");
    ne_buffer_zappend(header, "If-Match: 123321\r\n");
    ne_buffer_zappend(header, "If-Match: 123321\n");
    
    std::cerr << "here:" << header->data << std::endl;
}

void session_t::head(const std::string& raw_path, std::string& etag, time_t& mtime, off_t& length)
{
    std::shared_ptr<char> path(
        ne_path_escape(raw_path.c_str()),
        free
    );
    
    std::shared_ptr<ne_request> request(
        ne_request_create(p_->session.get(), "HEAD", path.get()),
        ne_request_destroy
    );
    
    int neon_stat = ne_request_dispatch(request.get());
    
    if (neon_stat != NE_OK) {
        std::cerr << "error when head:" << ne_get_error(p_->session.get()) << std::endl;
        throw std::runtime_error(ne_get_error(p_->session.get()));
    }
    
    const char *value = ne_get_response_header(request.get(), "ETag");
    if (value) {
        etag = normalize_etag(value);
    } else {
        etag.clear();
    }

    value = ne_get_response_header(request.get(), "Last-Modified");
    if (!value) value = ne_get_response_header(request.get(), "Date");
    if (value) {
        mtime = ne_httpdate_parse(value);
    } else {
        mtime = 0;
    }
    
    value = ne_get_response_header(request.get(), "Content-Length");
    if (value) {
        length = strtol(value, NULL, 10);
    } else {
        length = -1;
    }    
}



void session_t::get(const std::string& path_raw, ContentHandler& handler)
{
    std::shared_ptr<char> path(
        ne_path_escape(path_raw.c_str()),
        free
    );
    
   std::shared_ptr<ne_request> request(
        ne_request_create(p_->session.get(), "GET", path.get()),
        ne_request_destroy
    );

//     /* Allow compressed content by setting the header */
//     ne_add_request_header(request.get(), "Accept-Encoding", "gzip");
    
    /* hook called before the content is parsed to set the correct reader,
        * either the compressed- or uncompressed reader.
        */
//     ne_hook_post_headers(session, install_content_reader, write_ctx );
    
    ne_add_response_body_reader(request.get(), ne_accept_2xx, content_reader, &handler);    
    
    int neon_stat = ne_request_dispatch(request.get());
    
    if( neon_stat != NE_OK ) {
        std::cerr << "Error GET: Neon: %d, errno %d" <<  neon_stat <<  " no:" << errno;
    } else {
        const ne_status* status = ne_get_status(request.get());
        std::cerr << "GET http result %d (%s)" <<  status->code << " phrase:" << (status->reason_phrase ? status->reason_phrase : "<empty") << std::endl;
        if( status->klass != 2 ) {
            std::cerr << "sendfile request failed with http status %d!" <<  status->code;
        }
    }  
}



stat_t session_t::get(const std::string& path_raw, const QString& localpath)
{
    std::shared_ptr<char> path(
        ne_path_escape(path_raw.c_str()),
        free
    );
    
    const QString tmppath = localpath + ".davtmp";
    
    std::cerr << "g1" << std::endl;
    int fd = open((tmppath).toStdString().c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
    
    
//     int fd = open((tmppath).toStdString().c_str(), O_CREAT|O_WRONLY|O_TRUNC|OPEN_BINARY_FLAGS|O_LARGEFILE, 0644);
    
    
    std::cerr << "g2:" << fd << std::endl;
    
    stat_t data;
    
//         if (!*exists) {
//             ne_add_request_header(req, "If-None-Match", "*");
//         } else {
//             if (etag && *etag)
//                 ne_add_request_header(req, "If-Match", *etag);
//         }
    
    ne_hook_post_send(p_->session.get(), post_send_handler, &data);
    std::cerr << "g3:" << fd << std::endl;    
    int neon_stat = ne_get(p_->session.get(), path.get(), fd);
    std::cerr << "g4:" << fd << std::endl;
    ne_unhook_post_send(p_->session.get(), post_send_handler, &data);
    close(fd);
   
    std::cerr << "g5:" << fd << std::endl;
    if (neon_stat != NE_OK) {
        std::cerr << "error when get:" << ne_get_error(p_->session.get()) << std::endl;
        throw std::runtime_error(ne_get_error(p_->session.get()));
    }
    std::cerr << "g6:" << fd << std::endl;
    
    struct stat st;
    fstat(fd, &st);
    
    data.size = st.st_size;
    data.local_mtime = st.st_mtime;
    
    std::string etag;
    time_t mtime;
    off_t size = 0;
    head(path_raw, etag, mtime, size);
    
    if (data.etag.empty()) {
        data.etag = etag;
    }
    else if (etag != data.etag) {
        std::cerr << "etag differs " << data.etag << " remote:" << etag << std::endl;
    }    
    
    if (data.remote_mtime && data.remote_mtime != mtime) {
        std::cerr << "remote mtime differs " << data.remote_mtime << " remote:" << mtime << std::endl;
    }
    
    if (data.size != size) {
        std::cerr << "remote size differs " << data.size << " remote:" << size << std::endl;
    }
    
    return data;
}


void session_t::put(const std::string& path_raw, const std::vector<char>& buffer)
{
    std::shared_ptr<char> path(
        ne_path_escape(path_raw.c_str()),
        free
    );

    std::shared_ptr<ne_request> request(
        ne_request_create(p_->session.get(), "PUT", path.get()),
        ne_request_destroy
    );

    ne_set_request_body_buffer(request.get(), buffer.data(), buffer.size());

    int  neon_stat = ne_request_dispatch(request.get());

    if( neon_stat != NE_OK ) {
        std::cerr << "Error PUT: Neon: " <<  neon_stat <<  " errno:" << errno << " string:" << ne_get_error(p_->session.get()) << std::endl;
    } else {
        const ne_status* status = ne_get_status(request.get());
        std::cerr << "PUT http result %d (%s)" <<  status->code << " phrase:" << (status->reason_phrase ? status->reason_phrase : "<empty") << std::endl;
        if( status->klass != 2 ) {
            std::cerr << "sendfile request failed with http status %d!" <<  status->code;
        }
    }  
}

stat_t session_t::put(const std::string& path_raw, int fd)
{
    std::shared_ptr<char> path(
        ne_path_escape(path_raw.c_str()),
        free
    );

    stat_t data;

    
    ne_hook_pre_send(p_->session.get(), pre_send_handler, NULL);
    ne_hook_post_send(p_->session.get(), post_send_handler, &data);
    
    int neon_stat = ne_put(p_->session.get(), path.get(), fd);
    
    ne_unhook_post_send(p_->session.get(), post_send_handler, &data);
    ne_unhook_pre_send(p_->session.get(), pre_send_handler, NULL);
    
    std::cerr << "stat:" << neon_stat << std::endl;
    
    if (neon_stat != NE_OK) {
        std::cerr << "error when put:" << ne_get_error(p_->session.get()) << std::endl;
        throw std::runtime_error(ne_get_error(p_->session.get()));
    }
    
    return data;
}



action_processor_t::action_processor_t(session_t& session, db_t& db)
    : session_(session)
    , db_(db)
{

}

void action_processor_t::process(const action_t& action)
{
    
//             error         = 0,        
//         upload        = 1 << 0,
//         download      = 1 << 1,
//         local_changed = 1 << 2,
//         remote_changed= 1 << 3,
//         unchanged     = 1 << 4,
//         conflict      = 1 << 5,
//         both_deleted  = 1 << 6,
//         local_deleted = 1 << 7,
//         remote_deleted= 1 << 8,
//         upload_dir    = 1 << 9,
//         download_dir  = 1 << 10,

            std::cerr << "processing action: " << action.type << " " << action.localpath.toStdString() << " --> " << action.remotepath.toStdString() << std::endl;
            
    switch (action.type) {
        case action_t::upload: {

            int fd = open(action.localpath.toStdString().c_str(), O_RDWR);

            struct stat st;
            fstat(fd, &st);
            stat_t status = session_.put(action.remotepath.toStdString(), fd);
            close(fd);
            
            {
                int fd = open(action.localpath.toStdString().c_str(), O_RDWR);

                struct stat st;
                fstat(fd, &st);
                stat_t status = session_.put(action.remotepath.toStdString(), fd);
                close(fd);
            }
            
            status.size = st.st_size;
            status.local_mtime = st.st_mtime;
            
            if (status.etag.empty() || status.remote_mtime == 0) {
                std::string etag;
                time_t mtime;
                off_t size = 0;
                session_.head(action.remotepath.toStdString(), etag, mtime, size);

                if (status.etag.empty()) {
                    status.etag = etag;
                }
                else if (etag != status.etag) {
                    std::cerr << "etag differs " << status.etag << " remote:" << etag << std::endl;
                }    
                
                if (status.remote_mtime && status.remote_mtime != mtime) {
                    std::cerr << "remote mtime differs " << status.remote_mtime << " remote:" << mtime << std::endl;
                    status.remote_mtime = 0;
                }
                
                if (status.size != size) {
                    std::cerr << "remote size differs " << status.size << " remote:" << size << std::endl;
                }
            }
            
            db_entry_t& e = db_.entry(action.localpath);
            
            e.etag = status.etag;
            e.local_mtime = status.local_mtime;
            e.remote_mtime = status.remote_mtime;
            e.size = status.size;
            e.path = action.localpath;
            
            db_.save(action.localpath, e);
            
            break;
        }
        case action_t::download: {
            std::cerr << "d1" << std::endl;
            stat_t st =  session_.get(action.remotepath.toStdString(), action.localpath);
            std::cerr << "d2" << std::endl;
            break;
        }
        default: std::cerr << " == SKIP ==" << std::endl;
    };
}




