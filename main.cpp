
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>
#include <string.h>
#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <fcntl.h> 


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

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/concept_check.hpp>

#include <session.h>

static char *username = "";
static char *password = "";

static int auth(void *userdata, const char *realm, int attempt, char *user, char *pwd)
{
    std::cerr << "4" << std::endl;
    
    if (attempt) {
        return attempt;
    }

    std::cerr << "5" << std::endl;

    strncpy(user, username, NE_ABUFSIZ - 1);
    strncpy(pwd, password, NE_ABUFSIZ - 1);

    return 0;
}

static int
ssl_verify(void *userdata, int failures, const ne_ssl_certificate *cert)
{
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





struct dav_props {
    char *path;         /* The unescaped path of the resource. */
    char *name;         /* The name of the file or directory. Only the last
                           component (no path), no slashes. */
    char *etag;         /* The etag string, including quotation characters,
                           but without the mark for weak etags. */
    off_t size;         /* File size in bytes (regular files only). */
    time_t ctime;       /* Creation date. */
    time_t mtime;       /* Date of last modification. */
    int is_dir;         /* Boolean; 1 if a directory. */
    int is_exec;        /* -1 if not specified; 1 is executeable;
                           0 not executeable. */
    dav_props *next;    /* Next in the list. */
};


typedef struct {
    const char *path;           /* The *not* url-encoded path. */
    dav_props *results;         /* Start of the linked list of dav_props. */
} propfind_context;

void dav_delete_props(dav_props *props)
{
    if (props->path)
        free(props->path);
    if (props->name)
        free(props->name);
    if (props->etag)
        free(props->etag);
    free(props);
}

static int
get_ne_error(ne_session* session, const char *method)
{
    const char *text = "";//ne_get_error(session);

    char *tail;
    int err = strtol(text, &tail, 10);
    if (tail == text)
        return EIO;

    switch (err) {
        case 200:           /* OK */
        case 201:           /* Created */
        case 202:           /* Accepted */
        case 203:           /* Non-Authoritative Information */
        case 204:           /* No Content */
        case 205:           /* Reset Content */
        case 207:           /* Multi-Status */
        case 304:           /* Not Modified */
            return 0;
        case 401:           /* Unauthorized */
        case 402:           /* Payment Required */
        case 407:           /* Proxy Authentication Required */
            return EPERM;
        case 301:           /* Moved Permanently */
        case 303:           /* See Other */
        case 404:           /* Not Found */
        case 405:           /* Method Not Allowed */
        case 410:           /* Gone */
            return ENOENT;
        case 408:           /* Request Timeout */
        case 504:           /* Gateway Timeout */
            return EAGAIN;
        case 423:           /* Locked */
            return EACCES;
        case 400:           /* Bad Request */
        case 403:           /* Forbidden */
        case 409:           /* Conflict */
        case 411:           /* Length Required */
        case 412:           /* Precondition Failed */
        case 414:           /* Request-URI Too Long */
        case 415:           /* Unsupported Media Type */
        case 424:           /* Failed Dependency */
        case 501:           /* Not Implemented */
            return EINVAL;
        case 413:           /* Request Entity Too Large */
        case 507:           /* Insufficient Storage */
            return ENOSPC;
        case 206:           /* Partial Content */
        case 300:           /* Multiple Choices */
        case 302:           /* Found */
        case 305:           /* Use Proxy */
        case 306:           /* (Unused) */
        case 307:           /* Temporary Redirect */
        case 406:           /* Not Acceptable */
        case 416:           /* Requested Range Not Satisfiable */
        case 417:           /* Expectation Failed */
        case 422:           /* Unprocessable Entity */
        case 500:           /* Internal Server Error */
        case 502:           /* Bad Gateway */
        case 503:           /* Service Unavailable */
        case 505:           /* HTTP Version Not Supported */
            return EIO;
        default:
            return EIO;
    }
}

static int
get_error(ne_session* session, int ret, const char *method)
{
    int err;
    switch (ret) {
    case NE_OK:
    case NE_ERROR:
        err = get_ne_error(session, method);
        break;
    case NE_LOOKUP:
        err = EIO;
        break;
    case NE_AUTH:
        err = EPERM;
        break;
    case NE_PROXYAUTH:
        err = EPERM;
        break;
    case NE_CONNECT:
        err = EAGAIN;
        break;
    case NE_TIMEOUT:
        err = EAGAIN;
        break;
    case NE_FAILED:
        err = EINVAL;
        break;
    case NE_RETRY:
        err = EAGAIN;
        break;
    case NE_REDIRECT:
        err = ENOENT;
        break;
    default:
        err = EIO;
        break;
    }

    return err;
}


static void
ls_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set)
{
    std::vector<std::string>* res = reinterpret_cast<std::vector<std::string>*>(userdata); 

    assert(res);
    assert(uri);
    assert(set);
    
    //FIXME check
    //if (!res || !uri || !uri->path || !set) return;

    res->push_back(uri->path);
    
    return;
}

int main(int argc, char** argv)
{
    try {
    
        if (ne_sock_init() != 0)
            throw std::runtime_error("Can't init");

        std::cerr << "1" << std::endl;
        
        
        session_t session("https", "webdav.yandex.ru");
        
        std::cerr << "2" << std::endl;
        
        ne_add_server_auth(session.session(), NE_AUTH_ALL, auth, NULL);

        std::cerr << "3" << std::endl;


        ne_ssl_set_verify(session.session(), ssl_verify, NULL);
        ne_ssl_trust_default_ca(session.session());

        std::cerr << "4" << std::endl;
        

        char *spath = ne_path_escape("/");
        ne_server_capabilities caps = {0, 0, 0};
        int ret = ne_options(session.session(), spath, &caps);
        
        if (ret) {
    //         get_error(session, ret, "PROPFIND");
            std::cerr << ne_get_error(session.session()) << std::endl;;
            throw std::runtime_error("Can't ne_options");
        }


        if (caps.dav_class1 ) {
            std::cerr << "class 1" << std::endl;
        }

        if (caps.dav_class2 ) {
            std::cerr << "class 2" << std::endl;
        }
        

        free(spath);


        propfind_context ctx;
        ctx.path = "/";
        ctx.results = NULL;

        std::cerr << "10" << std::endl;
        
        char *espath = ne_path_escape("/testf/games");

        auto l = session.ls(espath);
        
        std::cerr << "a1" << std::endl;
        BOOST_FOREACH(auto s, l) {
            std::cerr << "file:" << s << std::endl;
        }
        
        std::cerr << "a2" << std::endl;
        
//         BOOST_FOREACH(auto s, get_res(session.session(), espath)) {
//             std::cerr << "file path:" << s.path << std::endl;
//             std::cerr << "file name:" << s.name << std::endl;
// //             std::cerr << "file stat:" << s.size << std::endl;
// //             std::cerr << "file dir:" << s.dir << std::endl;
// //             std::cerr << "file ctime:" << s.ctime << std::endl;
// //             std::cerr << "file mtime:" << s.mtime << std::endl;
// //             std::cerr << "file exec:" << s.exec << std::endl;
// //             std::cerr << "file etag:" << s.etag << std::endl;
//         }
//         
        std::cerr << "a3" << std::endl;
        
    //     std::cerr << "11" << std::endl;
    //     
    //     ne_propfind_handler *ph = ne_propfind_create(session, espath, NE_DEPTH_ONE);
    // 
    //     std::cerr << "12" << std::endl;
    //     
    //     ret = ne_propfind_named(ph, prop_names, prop_result, &ctx);
    // 
    //     std::cerr << "13" << std::endl;
    //     
    // //     ret = get_error(ret, "PROPFIND");
    //     ne_propfind_destroy(ph);
        free(espath);

        std::cerr << "14" << std::endl;
        
        if (ret) {
            std::cerr << "15" << std::endl;
            while(ctx.results) {
                std::cerr << "16" << std::endl;
                dav_props *tofree = ctx.results;
                std::cerr << "17" << std::endl;
                ctx.results = ctx.results->next;
                std::cerr << "18" << std::endl;
                dav_delete_props(tofree);
                std::cerr << "19" << std::endl;
            }
        }

        std::cerr << "20" << std::endl;
        
        int f = open("/tmp/333", O_RDWR | O_CREAT | O_TRUNC);
        
        session_t::ContentHandler writer = [f] (const char* buf, size_t len) {return write(f, buf, len);};
        
        session.get("/testf/games/русские буквы.txt", writer);
        
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








