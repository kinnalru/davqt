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
        std::cerr << "Error:" << err << std::endl;
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




