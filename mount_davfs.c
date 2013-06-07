/*  mount_davfs.c: mount the davfs file system.
    Copyright (C) 2006, 2007, 2008, 2009 Werner Baumann

    This file is part of davfs2.

    davfs2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    davfs2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with davfs2; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA. */


#include "config.h"

#ifdef HAVE_ARGZ_H
#include <argz.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <error.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <fstab.h>
#include <getopt.h>
#include <grp.h>
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <ne_string.h>
#include <ne_uri.h>
#include <ne_utils.h>

#include "defaults.h"
#include "mount_davfs.h"
#include "kernel_interface.h"
#include "cache.h"
#include "webdav.h"

#ifdef ENABLE_NLS
#define _(String) gettext(String)
#else
#define _(String) String
#define textdomain(Domainname)
#define bindtextdomain(Domainname, Dirname)
#endif


/* Private global variables */
/*==========================*/

/* The URL of the WebDAV server as taken from commandline.  */
static char *url;

/* The mointpoint as taken from the command line. */
static char *mpoint;

/* The type of the kernel file system used. */
static char *kernel_fs;

/* The file that holds information about mounted filesystems
   (/proc/mounts or /etc/mtab) */
static char *mounts;

/* The PID file */
static char *pidfile;

/* This flags signals the message loop of the kernel interface whether it
   shall do what it says or just stop. Will be reset by termination_handler. */
static volatile int keep_on_running = 1;

/* This flag signals that SIGTERM was received. mount.davfs will then
   terminate without uploading dirty files. */
static volatile int got_sigterm;


/* Private function prototypes */
/*=============================*/

/* Parsing, checking and mounting. */

static void
change_persona(dav_args *args);

static void
check_dirs(dav_args *args);

static char *
check_double_mounts(dav_args *args);

static void
check_fstab(const dav_args *args);

static void
check_mountpoint(dav_args *args);

static void
check_permissions(dav_args *args);

static int
do_mount(unsigned long int mopts, void *mdata);

static int
is_mounted(void);

static dav_args *
parse_commandline(int argc, char *argv[]);

static void
parse_config(char *argv[], dav_args *args);

static void
parse_secrets(dav_args *args);

static int
save_pid(void);

static void
termination_handler(int signo);

static void
write_mtab_entry(const dav_args *args);

/* Helper functions. */

static int
arg_to_int(const char *arg, int base, const char *opt);

static int
debug_opts(const char *s);

static int
debug_opts_neon(const char *s);

static char *
decode_octal(const char *s);

static void
delete_args(dav_args *args);

static void
eval_modes(dav_args *args);

static void
get_options(dav_args *args, char *option);

static dav_args *
new_args(void);

static void
log_dbg_cmdline(char *argv[]);

static void
log_dbg_config(char *argv[], dav_args *args);

static int
parse_line(char *line, int parmc, char *parmv[]);

static void
proxy_from_env(dav_args *args);

static void
read_config(dav_args *args, const char * filename, int system);

static void
read_no_proxy_list(dav_args *args);

static void
read_secrets(dav_args *args, const char *filename);

static int
split_uri(char **scheme, char **host, int *port,char **path, const char *uri);

static void
usage(void);

static char *
user_input(const char *prompt);


/* Public functions */
/*==================*/

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), PACKAGE_STRING);

    dav_args *args = parse_commandline(argc, argv);

    if (geteuid() != 0)
        error(EXIT_FAILURE, errno, _("program is not setuid root"));
    if (seteuid(getuid()) != 0)
        error(EXIT_FAILURE, errno, _("can't change effective user id"));

    if (getuid() != 0)
        check_fstab(args);

    parse_config(argv, args);

    check_mountpoint(args);

    check_dirs(args);

    check_permissions(args);

    parse_secrets(args);

    pidfile = check_double_mounts(args);

    change_persona(args);

    dav_init_webdav(args);

    dav_init_cache(args, mpoint);

    int dev = 0;
    dav_run_msgloop_fn run_msgloop = NULL;
    void *mdata = NULL;
    if (args->kernel_fs)
        kernel_fs = ne_strdup(args->kernel_fs);
    size_t buf_size = args->buf_size * 1024;
    int mounted = dav_init_kernel_interface(&dev, &run_msgloop, &mdata,
                                            &kernel_fs, &buf_size, url, mpoint,
                                            args);
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "kernel_fs: %s", kernel_fs);

    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Fork into daemon mode");
    pid_t childpid = fork();
    if (childpid > 0) {

        if (args->debug & DAV_DBG_CONFIG)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
                   "Parent: parent pid: %i, child pid: %i", getpid(), childpid);
        if (!mounted) {
            if (args->debug & DAV_DBG_CONFIG)
                syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
                       "Parent: mounting filesystem");
            if (do_mount(args->mopts, mdata) != 0) {
                kill(childpid, SIGTERM);
                delete_args(args);
                exit(EXIT_FAILURE);
            }
        }

        if (args->debug & DAV_DBG_CONFIG)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
                   "Parent: writing mtab entry");
        write_mtab_entry(args);

        if (args->debug & DAV_DBG_CONFIG)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Parent: leaving now");
        delete_args(args);
        exit(EXIT_SUCCESS);

    } else if (childpid < 0) {
        delete_args(args);
        error(EXIT_FAILURE, errno, _("can't start daemon process"));
    }

    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Set signal handler");
    struct sigaction action;
    action.sa_handler = termination_handler;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaddset(&action.sa_mask, SIGHUP);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    int ret = 0;

    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Releasing root privileges");
    uid_t daemon_id = geteuid();
    seteuid(0);
    ret = setuid(daemon_id);
    if (ret) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("can't release root privileges"));
        kill(getppid(), SIGHUP);
    }

    time_t idle_time = args->delay_upload;
    if (!idle_time)
        idle_time = DAV_DELAY_UPLOAD;
    int debug = args->debug;
    delete_args(args);
    setsid();
    if (!ret) {
        if (debug & DAV_DBG_CONFIG)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Releasing terminal");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        if (open("/dev/null", O_RDONLY) != 0 || open("/dev/null", O_WRONLY) != 1
                || open("/dev/null", O_WRONLY) != 2) {
            ret = -1;
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
                   _("failed to release tty properly"));
            kill(getppid(), SIGHUP);
        }
        dav_set_no_terminal();
    }

    if (!ret) {
        if (debug & DAV_DBG_CONFIG)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Writing pid file");
        ret = save_pid();
        if (ret) {
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
                   _("can't write pid file %s"), pidfile);
            kill(getppid(), SIGHUP);
        }
    }

    if (!ret) {
        if (debug & DAV_DBG_CONFIG)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Starting message loop");
        run_msgloop(dev, buf_size, idle_time, is_mounted, &keep_on_running,
                    debug & DAV_DBG_KERNEL);
    }

    if (debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Closing");
    dav_close_cache(got_sigterm);
    dav_close_webdav();
    if (is_mounted()) {
        char *prog = ne_concat("/bin/umount -il ", mpoint, NULL);
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("unmounting %s"), mpoint);
        if (system(prog) != 0)
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("unmounting failed"));
    }
    if (debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Removing %s", pidfile);
    remove(pidfile);
    if (debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Done.");
    return 0;
}


char *
dav_user_input_hidden(const char *prompt)
{
    printf("  %s ", prompt);

    struct termios old;
    if (tcgetattr(fileno(stdin), &old) != 0)
        return NULL;
    struct termios new = old;
    new.c_lflag &= ~ECHO;
    if (tcsetattr(fileno(stdin), TCSAFLUSH, &new) != 0)
        return NULL;
    char *line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, stdin);
    if (tcsetattr(fileno(stdin), TCSAFLUSH, &old) != 0)
        return NULL;
    printf("\n");

    if (len < 0) abort();
    if (len > 0 && *(line + len - 1) == '\n')
        *(line + len - 1) = '\0';

    return line;
}


/* Private functions */
/*===================*/

/* Changes the group id of the process permanently to dav_group. The
   effective user id of the process will be changed too, but the real
   user id still has to be changed permanently. */
static void
change_persona(dav_args *args)
{
    struct group *grp = getgrnam(args->dav_group);
    if (!grp)
        error(EXIT_FAILURE, errno, _("group %s does not exist"),
              args->dav_group);
    seteuid(0);
    if (setgid(grp->gr_gid) != 0)
        error(EXIT_FAILURE, errno, _("can't change group id"));

    if (getuid() == 0) {
        struct passwd *pw = getpwnam(args->dav_user);
        if (!pw)
            error(EXIT_FAILURE, errno, _("user %s does not exist"),
                  args->dav_user);
        if (seteuid(pw->pw_uid) != 0)
            error(EXIT_FAILURE, errno, _("can't change effective user id"));
    
    } else {
        if (seteuid(getuid()) != 0)
            error(EXIT_FAILURE, errno, _("can't change effective user id"));
    }
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "changing persona: euid %i, gid %i", geteuid(), getgid());
}


/* Checks for the existence of necessary and usefull directories and files.
   - checks whether it can use the proc file system for information about
     mounted file systems, or has to use mtab
   - whether the directory to save pid files exists and has correct owner and
     permissions; if not it tries to create it and/or set owner and permissions
   - when invoked by non-root user: checks for configuration directory in the
     users homepage and creates missing directories and files
   - checks wether args->cache_dir is accessible. */
static void
check_dirs(dav_args *args)
{
    struct stat st;

    if (access(DAV_MOUNTS, R_OK) == 0) {
        mounts = DAV_MOUNTS;
    } else {
        mounts = _PATH_MOUNTED;
    }
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "mounts in: %s", mounts);

    seteuid(0);
    if (access(DAV_SYS_RUN, F_OK) != 0) {
        if (mkdir(DAV_SYS_RUN, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISVTX)
                != 0)
            error(EXIT_FAILURE, errno, _("can't create directory %s"),
                  DAV_SYS_RUN);
    }
    if (stat(DAV_SYS_RUN, &st) != 0)
        error(EXIT_FAILURE, errno, _("can't access directory %s"),
              DAV_SYS_RUN);
    if ((st.st_mode & (S_IRWXG | S_ISVTX)) != (S_IRWXG | S_ISVTX)) {
        if (chmod(DAV_SYS_RUN, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISVTX)
                != 0)
            error(EXIT_FAILURE, errno,
                  _("can't change mode of directory %s"), DAV_SYS_RUN);
    }
    struct group *grp = getgrnam(args->dav_group);
    if (!grp)
        error(EXIT_FAILURE, errno, _("group %s does not exist"),
              args->dav_group);
    if (st.st_gid != grp->gr_gid) {
        if (chown(DAV_SYS_RUN, 0, grp->gr_gid) != 0)
            error(EXIT_FAILURE, errno,
                  _("can't change group of directory %s"), DAV_SYS_RUN);
    }
    seteuid(getuid());

    if (getuid() != 0) {

        char *path = NULL;
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir)
            path = ne_concat(pw->pw_dir, "/.", PACKAGE, NULL);
        if (path && access(path, F_OK) != 0)
            mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

        if (path && access(path, F_OK) == 0) {
            char *dir = ne_concat(path, "/", DAV_CACHE, NULL);
            if (access(dir, F_OK) != 0)
                mkdir(dir, S_IRWXU);
            free(dir);

            dir = ne_concat(path, "/", DAV_CERTS_DIR, NULL);
            if (access(dir, F_OK) != 0)
                mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            free(dir);

            dir = ne_concat(path, "/", DAV_CERTS_DIR, "/", DAV_CLICERTS_DIR,
                            NULL);
            if (access(dir, F_OK) != 0)
                mkdir(dir, S_IRWXU);
            free(dir);

            char *file_name = ne_concat(path, "/", DAV_CONFIG, NULL);
            if (access(file_name, F_OK) != 0) {
                char *template = ne_concat(DAV_DATA_DIR, "/", DAV_CONFIG, NULL);
                char *command = ne_concat("cp ", template, " ", file_name,
                                          NULL);
                if (system(command) != 0);
                free(command);
                free(template);
            }
            free(file_name);

            file_name = ne_concat(path, "/", DAV_SECRETS, NULL);
            if (access(file_name, F_OK) != 0) {
                char *template = ne_concat(DAV_DATA_DIR, "/", DAV_SECRETS,
                                           NULL);
                char *command = ne_concat("cp ", template, " ", file_name,
                                          NULL);
                if (system(command) == 0)
                    chmod(file_name, S_IRUSR | S_IWUSR);
                free(command);
                free(template);
            }
            free(file_name);
        }
        free(path);
    }

    if (strcmp(args->cache_dir, args->sys_cache) == 0) {

        seteuid(0);
        if (access(args->sys_cache, F_OK) != 0) {
            if (mkdir(args->sys_cache, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
                    != 0)
                error(EXIT_FAILURE, errno, _("can't create directory %s"),
                      args->sys_cache);
        }
        if (stat(args->sys_cache, &st) != 0)
            error(EXIT_FAILURE, errno, _("can't access directory %s"),
                  args->sys_cache);
        if ((st.st_mode & S_IRWXG) != S_IRWXG) {
            if (chmod(args->sys_cache, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
                    != 0)
                error(EXIT_FAILURE, errno,
                      _("can't change mode of directory %s"),
                      args->sys_cache);
        }
        struct group *grp = getgrnam(args->dav_group);
        if (!grp)
            error(EXIT_FAILURE, errno, _("group %s does not exist"),
                  args->dav_group);
        if (st.st_gid != grp->gr_gid) {
            if (chown(args->sys_cache, 0, grp->gr_gid) != 0)
                error(EXIT_FAILURE, errno,
                      _("can't change group of directory %s"),
                      args->sys_cache);
        }
        seteuid(getuid());

    } else {

        struct passwd *pw = getpwuid(getuid());
        if (!pw)
            error(EXIT_FAILURE, errno, _("can't read user data base"));
        if (!pw->pw_name)
            error(EXIT_FAILURE, 0, _("can't read user data base"));
        if (access(args->cache_dir, F_OK) != 0) {
            if (mkdir(args->cache_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
                    != 0)
                error(EXIT_FAILURE, errno, _("can't create directory %s"),
                      args->cache_dir);
        }
        if (stat(args->cache_dir, &st) != 0)
            error(EXIT_FAILURE, errno, _("can't access directory %s"),
                  args->cache_dir);
        if ((st.st_uid != getuid() || (st.st_mode & S_IRWXU) != S_IRWXU)
                &&  (st.st_mode & S_IRWXO) != S_IRWXO) {
            if ((st.st_mode & S_IRWXG) != S_IRWXG)
                error(EXIT_FAILURE, errno, _("can't access directory %s"),
                      args->cache_dir);
            struct group *grp = getgrgid(st.st_gid);
            if (!grp)
                error(EXIT_FAILURE, errno, _("can't read group data base"));
            char **members = grp->gr_mem;
            while (*members && strcmp(*members, pw->pw_name) != 0)
                members++;
            if (!*members)
                error(EXIT_FAILURE, 0, _("can't access directory %s"),
                args->cache_dir);
        }

    }
}


/* Checks whether url is already mounted on mpoint, creates the name of the
   pid file and checks whether it already exists.
   If one of these tests is positive, it prints an error message and
   terminates the program. Otherwise it returns the name of the pid file.
   return value : the name of the pid file. */
static char *
check_double_mounts(dav_args *args)
{
    FILE *mtab = setmntent(mounts, "r");
    if (!mtab)
        error(EXIT_FAILURE, errno, _("can't open file %s"), mounts);
    struct mntent *mt = getmntent(mtab);
    while (mt) {
        if (strcmp(mpoint, mt->mnt_dir) == 0
                && strcmp(url, mt->mnt_fsname) == 0)
            error(EXIT_FAILURE, 0, _("%s is already mounted on %s"), url,
                  mpoint);
        mt = getmntent(mtab);
    }
    endmntent(mtab);

    char *m = mpoint;
    while (*m == '/')
        m++;
    char *mp = ne_strdup(m);
    m = strchr(mp, '/');
    while (m) {
        *m = '-';
        m = strchr(mp, '/');
    }
    char *pidf = NULL;
    if (asprintf(&pidf, "%s/%s.pid", DAV_SYS_RUN, mp) < 0) abort();
    free(mp);
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "PID file: %s", pidf);

    FILE *file = fopen(pidf, "r");
    if (file)
        error(EXIT_FAILURE, 0, _("found PID file %s.\n"
              "Either %s is used by another process,\n"
              "or another mount process ended irregular"), pidf, mpoint);

    return pidf;
}


/* Checks fstab whether there is an entry for the mountpoint specified in args
   and compares the values in args with the values in fstab.
   If there is no such entry, or this entry does not allow user-mount, or the
   values differ, an error message is printed and the program terminates. */
static void
check_fstab(const dav_args *args)
{
    dav_args *n_args = new_args();
    n_args->mopts = DAV_USER_MOPTS;
    setfsent();
    struct fstab *ft = getfsfile(mpoint);
 
    if (!ft) {
        char *mp = NULL;
        if (asprintf(&mp, "%s/", mpoint) < 0) abort();
        ft = getfsfile(mp);
        if (mp) free(mp);
    }

   if (!ft || !ft->fs_spec)
        error(EXIT_FAILURE, 0, _("no entry for %s found in %s"), url,
              _PATH_FSTAB);

    if (strcmp(url, ft->fs_spec) != 0) {
        char *fstab_url = decode_octal(ft->fs_spec);
        if (strcmp(url, fstab_url) != 0)
            error(EXIT_FAILURE, 0, _("different URL in %s"), _PATH_FSTAB);
        free(fstab_url);
    }

    if (ft->fs_mntops)
        get_options(n_args, ft->fs_mntops);

    if (! ft->fs_vfstype || strcmp(DAV_FS_TYPE, ft->fs_vfstype) != 0)
        error(EXIT_FAILURE, 0, _("different file system type in %s"),
              _PATH_FSTAB);
    if (args->conf || n_args->conf) {
        if (!args->conf || !n_args->conf
                || strcmp(args->conf, n_args->conf) != 0)
            error(EXIT_FAILURE, 0, _("different config file in %s"),
                  _PATH_FSTAB);
    }
    if (args->cl_username || n_args->cl_username) {
        if (!args->cl_username || !n_args->cl_username
                || strcmp(args->cl_username, n_args->cl_username) != 0)
            error(EXIT_FAILURE, 0, _("different username in %s"), _PATH_FSTAB);
    }
    if (!n_args->user && !n_args->users)
        error(EXIT_FAILURE, 0,
              _("neither option `user' nor option `users' set in %s"),
              _PATH_FSTAB);
    if (args->user != n_args->user)
        error(EXIT_FAILURE, 0, _("different option `user' in %s"), _PATH_FSTAB);
    if (args->users != n_args->users)
        error(EXIT_FAILURE, 0, _("different option `users' in %s"),
              _PATH_FSTAB);
    if (args->mopts != n_args->mopts)
        error(EXIT_FAILURE, 0, _("different mount options in %s"),
              _PATH_FSTAB);
    if (args->uid != n_args->uid)
        error(EXIT_FAILURE, 0, _("different uid in %s"), _PATH_FSTAB);
    if (args->gid != n_args->gid)
        error(EXIT_FAILURE, 0, _("different gid in %s"), _PATH_FSTAB);
    if (args->dir_mode != n_args->dir_mode)
        error(EXIT_FAILURE, 0, _("different dir_mode in %s"), _PATH_FSTAB);
    if (args->file_mode != n_args->file_mode)
        error(EXIT_FAILURE, 0, _("different file_mode in %s"), _PATH_FSTAB);

    endfsent();
    delete_args(n_args);
}


/* Checks if a valid mountpoint is specified.
   For non root users this must meet the additional conditions:
   - if the mount point is given as relative path, it must lie within
     the mounting users home directory (so a relative path in fstab
     - which might be useful in some cases - will not allow to to
     gain access to directories not intended).
   - if given as an absulute path it must not lie within another users
     home directory (even the administrator can't give the right to mount
     into another users home directory).
   If this conditions are not met or an error occurs, an error message is
   printed and exit(EXIT_FAILURE) is called. */
static void
check_mountpoint(dav_args *args)
{

    struct passwd *pw;

    if (*mpoint != '/') {
        char *mp = canonicalize_file_name(mpoint);
        if (!mp)
            error(EXIT_FAILURE, 0,
                  _("can't evaluate path of mount point %s"), mpoint);
        if (getuid() != 0) {
            pw = getpwuid(getuid());
            if (!pw || !pw->pw_dir)
                error(EXIT_FAILURE, 0,
                      _("can't get home directory for uid %i"), getuid());
            if (strstr(mp, pw->pw_dir) != mp)
                error(EXIT_FAILURE, 0, _("A relative mount point must lie "
                      "within your home directory"));
        }
        free(mpoint);
        mpoint = mp;
    }

    if (strcmp(mpoint, "/") == 0 || strlen(mpoint) < 2)
        error(EXIT_FAILURE, 0, _("invalid mount point %s"), mpoint);

    if (access(mpoint, F_OK) != 0)
        error(EXIT_FAILURE, 0, _("mount point %s does not exist"), mpoint);

    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "mountpoint: %s", mpoint);
}


/* The mounting user must be either root or meet the following conditions:
   - The  uid must not differ from the option uid, if this option is used.
   - The user must belong to the group specified in option gid (if used).
   - The user must be member of group args->dav_group.
   If this conditions are not met or an error occurs, an error message is
   printed and exit(EXIT_FAILURE) is called. */
static void
check_permissions(dav_args *args)
{
    if (getuid() == 0)
        return;

    if (args->uid != getuid())
        error(EXIT_FAILURE, 0,
              _("you can't set file owner different from your uid"));
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "uid ok");

    if (getgid() != args->gid) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw)
            error(EXIT_FAILURE, errno, _("can't read user data base"));
        if (!pw->pw_name)
            error(EXIT_FAILURE, 0, _("can't read user data base"));
        struct group *grp = getgrgid(args->gid);
        if (!grp)
            error(EXIT_FAILURE, 0, _("can't read group data base"));
        char **members = grp->gr_mem;
        while (*members && strcmp(*members, pw->pw_name) != 0)
            members++;
        if (!*members)
            error(EXIT_FAILURE, 0,
                  _("you must be member of the group of the file system"));
    }
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "gid ok");

    struct passwd *pw;
    pw = getpwuid(getuid());
    if (!pw)
        error(EXIT_FAILURE, errno, _("can't read user data base"));
    if (!pw->pw_name)
        error(EXIT_FAILURE, 0, _("can't read user data base"));
    struct group *grp = getgrnam(args->dav_group);
    if (!grp)
        error(EXIT_FAILURE, errno, _("group %s does not exist"),
              args->dav_group);
    if (pw->pw_gid != grp->gr_gid) {
        int ngroups = getgroups(0, NULL);
        gid_t *groups = NULL;
        if (ngroups > 0) {
            groups = (gid_t *) malloc(ngroups * sizeof(gid_t));
            if (!groups) abort();
            if (getgroups(ngroups, groups) < 0)
                error(EXIT_FAILURE, 0, _("can't read group data base"));
        } else {
            error(EXIT_FAILURE, 0, _("can't read group data base"));
        }
        int i;
        for (i = 0; i < ngroups; i++) {
            if (grp->gr_gid == groups[i])
                break;
        }
        free(groups);
        if (i == ngroups)
            error(EXIT_FAILURE, 0, _("user %s must be member of group %s"),
                  pw->pw_name, grp->gr_name);
    }
    if (args->debug & DAV_DBG_CONFIG)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "memeber of group %s", args->dav_group);
}


/* Calls the mount()-function to mount the file system.
   Uses private global variables url and mpoint as device and mount point,
   kernel_fs as file system type, mopts as mount options and mdata
   as mount data.
   return value : 0 on success, -1 if mount() fails. */
static int
do_mount(unsigned long int mopts, void *mdata)
{
    uid_t orig = geteuid();
    seteuid(0);
    int ret = mount(url, mpoint, kernel_fs,  mopts, mdata);
    seteuid(orig);

    if (ret) {
        error(0, errno, _("can't mount %s on %s"), url, mpoint);
        if (errno == ENODEV)
            error(0, 0, _("kernel does not know file system %s"), kernel_fs);
        if (errno == EBUSY)
            error(0, 0, _("mount point is busy"));
        return -1;
    }
    return 0;
}


/* Checks wether the file system is mounted.
   It uses information from the private global variables mounts (mtab-file),
   url (must be device in the mtab entry) and mpoint (mount point).
   return value : 0 - no matching entry in the mtab-file (not mounted)
                  1 - matching entry in the mtab-file (mounted) */
static int
is_mounted(void)
{
    int found = 0;
    FILE *mtab = setmntent(mounts, "r");
    if (mtab) {
        struct mntent *mt = getmntent(mtab);
        while (mt && !found) {
            if (strcmp(mpoint, mt->mnt_dir) == 0
                        && strcmp(url, mt->mnt_fsname) == 0)
                found = 1;
            mt = getmntent(mtab);
        }
    }
    endmntent(mtab);
    return found;
}

/* Parses commandline arguments and options and stores them in args and the
   private global variables url and mpoint.
   For arguments and options please see the usage()-funktion.
   As soon as 'version' or 'help' is found, an appropriate message is printed
   and exit(EXIT_SUCCESS) is called.
   If it does not find exactly two non-option-arguments (url and mointpoint)
   it prints an error message and calls exit(EXIT_FAILURE).
   argc    : the number of arguments.
   argv[]  : array of argument strings.
   return value : args, containig the parsed options and arguments. The args
                  structure and all strings are newly allocated. The calling
                  function is responsible to free them. */
static dav_args *
parse_commandline(int argc, char *argv[])
{
    log_dbg_cmdline(argv);
    dav_args *args = new_args();

    char *short_options = "vwVho:";
    static const struct option options[] = {
        {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},
        {"option", required_argument, NULL, 'o'},
        {0, 0, 0, 0}
    };

    int o;
    o = getopt_long(argc, argv, short_options, options, NULL);
    while (o != -1) {
        switch (o) {
        case 'V':
            printf("%s  <%s>\n\n", PACKAGE_STRING, PACKAGE_BUGREPORT);
            printf(_("This is free software; see the source for copying "
                     "conditions.  There is NO\n"
                     "warranty; not even for MERCHANTABILITY or FITNESS "
                     "FOR A PARTICULAR PURPOSE.\n"));
            exit(EXIT_SUCCESS);
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        case 'o':
            get_options(args, optarg);
            break;
        case 'v':
        case 'w':
        case '?':
            break;
        default:
            error(EXIT_FAILURE, 0, _("unknown error parsing arguments"));
        }
        o = getopt_long(argc, argv, short_options, options, NULL);
    }

    int i = optind;
    switch (argc - i) {
    case 0:
    case 1:
        error(0, 0, _("missing argument"));
        usage();
        exit(EXIT_FAILURE);
    case 2:
        if (*argv[i] == '\"' || *argv[i] == '\'') {
            url = ne_strndup(argv[i] + 1, strlen(argv[i]) -2);
        } else {
            url = ne_strdup(argv[i]);
        }
        i++;
        mpoint = ne_strdup(argv[i]);
        break;
    default:
        error(0, 0, _("too many arguments"));
        usage();
        exit(EXIT_FAILURE);
    }

    if (!mpoint)
        error(EXIT_FAILURE, 0, _("no mountpoint specified"));
    while (strlen(mpoint) > 1 && *(mpoint + strlen(mpoint) -1) == '/')
        *(mpoint + strlen(mpoint) -1) = '\0';

    if (!url)
        error(EXIT_FAILURE, 0, _("no WebDAV-server specified"));
    if (split_uri(&args->scheme, &args->host, &args->port, &args->path,
                  url) != 0)
        error(EXIT_FAILURE, 0, _("invalid URL"));
    if (!args->port)
        args->port = ne_uri_defaultport(args->scheme);

    return args;       
}


/* Reads and parses the configuration files and stores the values in args.
   The system wide configuration file is parsed first. If args->conf is
   given it will be parsed too and overwrites the values from the system
   wide configuration file. */
static void
parse_config(char *argv[], dav_args *args)
{
    read_config(args, DAV_SYS_CONF_DIR "/" DAV_CONFIG, 1);

    struct passwd *pw = getpwuid(getuid());
    if (!pw || !pw->pw_dir)
        error(EXIT_FAILURE, 0, _("can't determine home directory"));

    if (args->conf) {
        if (*args->conf == '~') {
            int p = 1;
            if (*(args->conf + p) == '/')
                p++;
            char *f = ne_concat(pw->pw_dir, "/", args->conf + p, NULL);
            free(args->conf);
            args->conf = f;
        }
        read_config(args, args->conf, 0);
    }

    args->mopts |= DAV_MOPTS;

    eval_modes(args);

    if (args->servercert && *args->servercert == '~') {
        int p = 1;
        if (*(args->servercert + p) == '/')
            p++;
        char *f = ne_concat(pw->pw_dir, "/", args->servercert + p, NULL);
        free(args->servercert);
        args->servercert = f;
    }
    if (args->servercert && *args->servercert != '/' && getuid() != 0) {
        char *f = ne_concat(pw->pw_dir, "/.", PACKAGE, "/", DAV_CERTS_DIR, "/",
                            args->servercert, NULL);
        if (access(f, F_OK) == 0) {
            free(args->servercert);
            args->servercert = f;
        } else {
            free(f);
        }
    }
    if (args->servercert && *args->servercert != '/') {
        char *f = ne_concat(DAV_SYS_CONF_DIR, "/", DAV_CERTS_DIR, "/",
                            args->servercert, NULL);
        free(args->servercert);
        args->servercert = f;
    }

    if (args->secrets && *args->secrets == '~') {
        int p = 1;
        if (*(args->secrets + p) == '/')
            p++;
        char *f = ne_concat(pw->pw_dir, "/", args->secrets + p, NULL);
        free(args->secrets);
        args->secrets = f;
    }

    if (args->clicert && *args->clicert == '~') {
        int p = 1;
        if (*(args->clicert + p) == '/')
            p++;
        char *f = ne_concat(pw->pw_dir, "/", args->clicert + p, NULL);
        free(args->clicert);
        args->clicert = f;
    }
    if (args->clicert && *args->clicert != '/' && getuid() != 0) {
        char *f = ne_concat(pw->pw_dir, "/.", PACKAGE, "/", DAV_CERTS_DIR, "/",
                            DAV_CLICERTS_DIR, "/", args->clicert, NULL);
        if (access(f, F_OK) == 0) {
            free(args->clicert);
            args->clicert = f;
        }
    }
    if (args->clicert && *args->clicert != '/' && getuid() == 0) {
        char *f = ne_concat(DAV_SYS_CONF_DIR, "/", DAV_CERTS_DIR, "/",
                            DAV_CLICERTS_DIR, "/", args->clicert, NULL);
        free(args->clicert);
        args->clicert = f;
    }
    if (args->clicert) {
        struct stat st;
        seteuid(0);
        if (stat(args->clicert, &st) < 0)
            error(EXIT_FAILURE, 0, _("can't read client certificate %s"),
                  args->clicert);
        seteuid(getuid());
        if (st.st_uid != getuid() && st.st_uid != 0)
            error(EXIT_FAILURE, 0,
                  _("client certificate file %s has wrong owner"),
                  args->clicert);
        if ((st.st_mode &
                (S_IXUSR | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX))
                != 0)
            error(EXIT_FAILURE, 0,
                  _("client certificate file %s has wrong permissions"),
                  args->clicert);
    }

    if (getuid() == 0 && !args->p_host) {
        proxy_from_env(args);
        read_no_proxy_list(args);
    }

    if (!args->p_host)
        args->useproxy = 0;

    if (!args->cache_dir) {
        args->cache_dir = ne_strdup(args->sys_cache);
    } else if (*args->cache_dir == '~') {
        int p = 1;
        if (*(args->cache_dir + p) == '/')
            p++;
        char *f = ne_concat(pw->pw_dir, "/", args->cache_dir + p, NULL);
        free(args->cache_dir);
        args->cache_dir = f;
    }

    if (args->debug & DAV_DBG_CONFIG)
        log_dbg_config(argv, args);
}


/* Reads the secrets file and asks the user interactivly for credentials if
   necessary. The user secrets file is parsed after the system wide secrets
   file, so it will have precedence. */
static void
parse_secrets(dav_args *args)
{
    seteuid(0);
    read_secrets(args, DAV_SYS_CONF_DIR "/" DAV_SECRETS);
    seteuid(getuid());

    if (args->secrets) {
        read_secrets(args, args->secrets);
    }

    if (args->cl_username) {
        if (args->username)
            free(args->username);
        args->username = args->cl_username;
        args->cl_username = NULL;
        if (args->password)
            free(args->password);
        args->password = NULL;
        args->password = user_input(_("Password: "));
    }

    if (args->askauth && args->useproxy && !args->p_user) {
        printf(_("Please enter the username to authenticate with proxy\n"
                 "%s or hit enter for none.\n"), args->p_host);
        args->p_user = user_input(_("Username:"));
    }

    if (args->askauth && args->useproxy && args->p_user && !args->p_passwd) {
        printf(_("Please enter the password to authenticate user %s with proxy\n"
                 "%s or hit enter for none.\n"), args->p_user, args->p_host);
        if (isatty(fileno(stdin))) {
            args->p_passwd = dav_user_input_hidden(_("Password: "));
        } else {
            args->p_passwd = user_input(_("Password: "));
        }
        if (args->p_passwd && strlen(args->p_passwd) == 0) {
            free(args->p_passwd);
            args->p_passwd = NULL;
        }
    }

    if (args->askauth && !args->username) {
        printf(_("Please enter the username to authenticate with server\n"
                 "%s or hit enter for none.\n"), url);
        args->username = user_input(_("Username:"));
    }

    if (args->askauth && args->username && !args->password) {
        printf(_("Please enter the password to authenticate user %s with "
                 "server\n%s or hit enter for none.\n"), args->username, url);
        if (isatty(fileno(stdin))) {
            args->password = dav_user_input_hidden(_("Password: "));
        } else {
            args->password = user_input(_("Password: "));
        }
        if (args->password && strlen(args->password) == 0) {
            free(args->password);
            args->password = NULL;
        }
    }

    if (args->debug & DAV_DBG_SECRETS) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Secrets:");
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "  username: %s", args->username);
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "  cl_username: %s", args->cl_username);
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "  password: %s", args->password);
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "  p_user: %s", args->p_user);
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "  p_passwd: %s", args->p_passwd);
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
               "  clicert_pw: %s", args->clicert_pw);
    }
}


/* Saves the pid of the mount.davfs daemon in the pid-file. The name of the
   pid-file is taken from the private global variable pidfile. If an error
   occurs during opening of the pid-file, the function returns with -1. */
static int
save_pid(void)
{
    FILE *file = fopen(pidfile, "w");
    if (!file)
        return -1;
    int ret = 0;
    if (fprintf(file, "%i\n", getpid()) <= 0)
        ret = -1;
    fclose(file);
    chmod(pidfile, S_IRWXU | S_IRGRP | S_IROTH);
    return ret;
}


/* Signal handler for the daemon process.
   Sets global variable keep_on_running to 0, so the message loop will stop
   and the daemon will terminate gracefully. */
static void
termination_handler(int signo)
{
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("pid %i, got signal %i"),
                       getpid(), signo);

    keep_on_running = 0;
    got_sigterm = 1;
}


/* Adds an entry to _PATH_MOUNTED for the mounted file system.
   If this fails a warning will be printed, but this will not stop mounting. */
static void
write_mtab_entry(const dav_args *args)
{
    struct mntent mntent;
    mntent.mnt_fsname = url;
    mntent.mnt_dir = mpoint;
    mntent.mnt_type = DAV_FS_TYPE;
    mntent.mnt_opts = NULL;
    if (asprintf(&mntent.mnt_opts, "%s%s%s%s%s%s",
                 (args->mopts & MS_RDONLY) ? "ro" : "rw",
                 (args->mopts & MS_NOSUID) ? ",nosuid" : "",
                 (args->mopts & MS_NOEXEC) ? ",noexec" : "",
                 (args->mopts & MS_NODEV) ? ",nodev" : "",
                 (args->netdev) ? ",_netdev" : "",
                 (args->add_mopts != NULL) ? args->add_mopts : "") < 0)
        abort();
    mntent. mnt_freq = 0;
    mntent. mnt_passno = 0;

    if (getuid() != 0) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw) {
            error(0, errno, _("Warning: can't read user data base. Mounting "
                              "anyway, but there is no entry in mtab."));
            return;
        }
        char *opts = mntent.mnt_opts;
        mntent.mnt_opts = ne_concat(opts, ",user=", pw->pw_name, NULL);
        free(opts);
    }

    uid_t orig = geteuid();
    seteuid(0);
    int ret;
    FILE *mtab = setmntent(_PATH_MOUNTED, "a");
    if (mtab) {
        ret = addmntent(mtab, &mntent);
        endmntent(mtab);
    } else {
        error(0, 0, _("Warning: can't write entry into mtab, but will mount "
                      "the file system anyway"));
    }
    seteuid(orig);

    free(mntent.mnt_opts);
}


/* Helper functions. */

/* Searches arg from the beginning for digits, valid in base, and converts
   them to an integer. If arg does not start with valid digits an error
   message is printed and exit(EXIT_FAILURE) is called.
   Otherwise the integer is returned.
   arg    : string to be converted
   base   : radix of the number; value between 2 and 36
   opt    : name of the option, arg belongs to. Used in the error message.
   return value: the value of the integer number in arg */
static int
arg_to_int(const char *arg, int base, const char *opt)
{
    char *tail = NULL;
    int n = strtol(arg, &tail, base);
    if (n < 0 || !tail) {
        if (base == 10) {
            error(EXIT_FAILURE, 0, _("option %s has invalid argument;"
                                     "it must be a decimal number"), opt);
        } else if (base == 8) {
            error(EXIT_FAILURE, 0, _("option %s has invalid argument;"
                                     "it must be an octal number"), opt);
        } else {
            error(EXIT_FAILURE, 0, _("option %s has invalid argument;"
                                     "it must be a number"), opt);
        }
    }

    return n;
}


/* Converts a debug option string s into numerical value. If s is not a
   valid debug option, it returns 0. */
static int
debug_opts(const char *s)
{
    if (strcmp(s, "config") == 0)
        return DAV_DBG_CONFIG;
    if (strcmp(s, "kernel") == 0)
        return DAV_DBG_KERNEL;
    if (strcmp(s, "cache") == 0)
        return DAV_DBG_CACHE;
    if (strcmp(s, "secrets") == 0)
        return DAV_DBG_SECRETS;
    if (strcmp(s, "most") == 0)
        return DAV_DBG_CONFIG | DAV_DBG_KERNEL | DAV_DBG_CACHE;
    return 0;
}


/* Converts a debug option string s into numerical value. If s is not a
   valid neon debug option, it returns 0. */
static int
debug_opts_neon(const char *s)
{
    if (strcmp(s, "http") == 0)
        return NE_DBG_HTTP | NE_DBG_SOCKET;
    if (strcmp(s, "xml") == 0)
        return NE_DBG_XML;
    if (strcmp(s, "httpauth") == 0)
        return NE_DBG_HTTPAUTH;
    if (strcmp(s, "locks") == 0)
        return NE_DBG_LOCKS;
    if (strcmp(s, "httpbody") == 0)
        return NE_DBG_HTTPBODY;
    if (strcmp(s, "ssl") == 0)
        return NE_DBG_SSL;
    if (strcmp(s, "secrets") == 0)
        return NE_DBG_HTTPPLAIN;
    if (strcmp(s, "most") == 0)
        return NE_DBG_SOCKET | NE_DBG_HTTP;
    return 0;
}


/* Searches string s for octal encoded characters (like \040 for space).
   It returns a new string where these have been replaced by the
   respective characters. */
static char *
decode_octal(const char *s)
{
    const char *old = s;
    char *decoded = calloc(strlen(s) +1, 1);
    if (!decoded) abort();
    while (*old != '\0') {
        char *pos = strstr(old, "\\0");
        if (pos) {
            char *tail;
            int c = strtol(pos + 1, &tail, 8);
            if ((tail - pos) == 4 && c != 0 && c >= CHAR_MIN && c <= CHAR_MAX) {
                strncat(decoded, old, pos - old);
                *(decoded + strlen(decoded)) = c;
                old = tail;
            } else {
                pos += 2;
                strncat(decoded, old, pos - old);
                old = pos;
            }
        } else {
            strcat(decoded, old);
            old += strlen(old);
        }
    }
    
    return decoded;
}


/* Frees all strings held by args and finally frees args. */
static void
delete_args(dav_args *args)
{
    if (args->dav_user)
        free(args->dav_user);
    if (args->dav_group)
        free(args->dav_group);
    if (args->conf)
        free(args->conf);
    if (args->add_mopts)
        free(args->add_mopts);
    if (args->kernel_fs)
        free(args->kernel_fs);
    if (args->scheme)
        free(args->scheme);
    if (args->host)
        free(args->host);
    if (args->path)
        free(args->path);
    if (args->servercert)
        free(args->servercert);
    if (args->secrets)
        free(args->secrets);
    if (args->username) {
        memset(args->username, '\0', strlen(args->username));
        free(args->username);
    }
    if (args->cl_username)
        free(args->cl_username);
    if (args->password) {
        memset(args->password, '\0', strlen(args->password));
        free(args->password);
    }
    if (args->clicert)
        free(args->clicert);
    if (args->clicert_pw) {
        memset(args->clicert_pw, '\0', strlen(args->clicert_pw));
        free(args->clicert_pw);
    }
    if (args->p_host)
        free(args->p_host);
    if (args->p_user) {
        memset(args->p_user, '\0', strlen(args->p_user));
        free(args->p_user);
    }
    if (args->p_passwd) {
        memset(args->p_passwd, '\0', strlen(args->p_passwd));
        free(args->p_passwd);
    }
    if (args->lock_owner)
        free(args->lock_owner);
    if (args->s_charset)
        free(args->s_charset);
    if (args->header)
        free(args->header);
    if (args->sys_cache)
        free(args->sys_cache);
    if (args->cache_dir)
        free(args->cache_dir);
    if (args->backup_dir)
        free(args->backup_dir);
    free(args);
}


/* Evaluates the umask and thedefault modes for directories and files from
   args->mopts, umask(), args->dir_mode and args->file_mode and stores them
   in args. */
static void
eval_modes(dav_args *args)
{
    if (args->mopts & MS_NOSUID)
        args->dir_umask = S_ISUID | S_ISGID;
    if (args->mopts & MS_RDONLY)
        args->dir_umask |= S_IWUSR | S_IWGRP | S_IWOTH;

    args->file_umask = args->dir_umask;
    if (args->mopts & MS_NOEXEC)
        args->file_umask |= S_IXUSR | S_IXGRP | S_IXOTH;

    mode_t default_mode = umask(0);
    umask(default_mode);
    default_mode = ~default_mode;
    default_mode &= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

    if (!args->dir_mode) {
        args->dir_mode = default_mode;
        args->dir_mode |= (args->dir_mode & S_IRUSR) ? S_IXUSR : 0;
        args->dir_mode |= (args->dir_mode & S_IRGRP) ? S_IXGRP : 0;
        args->dir_mode |= (args->dir_mode & S_IROTH) ? S_IXOTH : 0;
    }
    args->dir_mode &= ~args->dir_umask;
    args->dir_mode |= S_IFDIR;

    if (!args->file_mode)
        args->file_mode = default_mode;
    args->file_mode &= ~args->file_umask;
    args->file_mode |= S_IFREG;
}


/* Parses the string option and stores the values in the appropriate fields of
   args. If an unknown option is found exit(EXIT_FAILURE) is called.
   All strings returned in args are newly allocated, and the calling function
   is responsible to free them.
   option : a comma separated list of options (like the options in fstab and
            in the -o option of the mount-programm).
            For known options see the declaration at the beginning of the
            the function definition. */
static void
get_options(dav_args *args, char *option)
{
    enum {
        CONF = 0,
        USERNAME,
        UID,
        GID,
        FILE_MODE,
        DIR_MODE,
        USER,
        NOUSER,
        USERS,
        NETDEV,
        NONETDEV,
        RW,
        RO,
        SUID,
        NOSUID,
        EXEC,
        NOEXEC,
        DEV,
        NODEV,
        ASYNC,
        AUTO,
        NOAUTO,
        DEFAULTS,
        END
    };
    char *suboptions[] = {
        [CONF] = "conf",
        [USERNAME] = "username",
        [UID] = "uid",
        [GID] = "gid",
        [FILE_MODE] = "file_mode",
        [DIR_MODE] = "dir_mode",
        [USER] = "user",
        [NOUSER] = "nouser",
        [USERS] = "users",
        [NETDEV] = "_netdev",
        [NONETDEV] = "no_netdev",
        [RW] = "rw",
        [RO] = "ro",
        [SUID] = "suid",
        [NOSUID] = "nosuid",
        [EXEC] = "exec",
        [NOEXEC] = "noexec",
        [DEV] = "dev",
        [NODEV] = "nodev",
        [ASYNC] = "async",
        [AUTO] = "auto",
        [NOAUTO] = "noauto",
        [DEFAULTS] = "defaults",
        [END] = NULL
    };

    int so;
    char *argument = NULL;
    struct passwd *pwd;
    struct group *grp;
    char *add_mopts = NULL;

    while (*option != 0) {
        so = getsubopt(&option, suboptions, &argument);
        if ((!argument) && (so < USER))
            error(EXIT_FAILURE, 0,
                 _("option %s requires argument"), suboptions[so]);
        switch (so) {
        case CONF:
            if (args->conf)
                free(args->conf);
            args->conf = ne_strdup(argument);
            break;
        case USERNAME:
            if (args->cl_username)
                free(args->cl_username);
            args->cl_username = ne_strdup(argument);
            break;
        case UID:
            pwd = getpwnam(argument);
            if (!pwd) {
                args->uid = arg_to_int(argument, 10, suboptions[so]);
            } else {
                args->uid = pwd->pw_uid;
            }
            if (asprintf(&add_mopts, "%s,uid=%i",
                         (args->add_mopts) ? args->add_mopts : "",
                         args->uid) < 0)
                abort();
            if (args->add_mopts)
                free(args->add_mopts);
            args->add_mopts = add_mopts;
            add_mopts = NULL;
            break;
        case GID:
            grp = getgrnam(argument);
            if (!grp) {
                args->gid = arg_to_int(argument, 10, suboptions[so]);
            } else {
                args->gid = grp->gr_gid;
            }
            if (asprintf(&add_mopts, "%s,gid=%i",
                         (args->add_mopts) ? args->add_mopts : "",
                         args->gid) < 0)
                abort();
            if (args->add_mopts)
                free(args->add_mopts);
            args->add_mopts = add_mopts;
            add_mopts = NULL;
            break;
        case FILE_MODE:
            args->file_mode = arg_to_int(argument, 8, suboptions[so]);
            break;
        case DIR_MODE:
            args->dir_mode = arg_to_int(argument, 8, suboptions[so]);
            break;
        case USER:
            args->user = 1;
            break;
        case NOUSER:
            args->user = 0;
            break;
        case USERS:
            args->users = 1;
            break;
        case NETDEV:
            args->netdev = 1;
            break;
        case NONETDEV:
            args->netdev = 0;
            break;
        case RW:
            args->mopts &= ~MS_RDONLY;
            break;
        case RO:
            args->mopts |= MS_RDONLY;
            break;
        case SUID:
            args->mopts &= ~MS_NOSUID;
            break;
        case NOSUID:
            args->mopts |= MS_NOSUID;
            break;
        case EXEC:
            args->mopts &= ~MS_NOEXEC;
            break;
        case NOEXEC:
            args->mopts |= MS_NOEXEC;
            break;
        case DEV:
            args->mopts &= ~MS_NODEV;
            break;
        case NODEV:
            args->mopts |= MS_NODEV;
            break;
        case ASYNC:
        case AUTO:
        case NOAUTO:
        case DEFAULTS:
            break;
        default:
            if (so == -1) {
                printf(_("Unknown option %s.\n"), argument);
                usage();
                exit(EXIT_FAILURE);
            }
        }
    }
}


/* Allocates a new dav_args-structure and initializes it.
   All members are set to reasonable defaults. */
static dav_args *
new_args(void)
{
    char *user_dir = NULL;
    if (getuid() != 0) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw)
            error(EXIT_FAILURE, errno, _("can't read user data base"));
        if (!pw->pw_dir)
            error(EXIT_FAILURE, 0, _("can't read user data base"));
        user_dir = ne_concat(pw->pw_dir, "/.", PACKAGE, NULL);
    }

    dav_args *args = ne_malloc(sizeof(*args));

    args->dav_user = ne_strdup(DAV_USER);
    args->dav_group = ne_strdup(DAV_GROUP);

    if (getuid() != 0) {
        args->conf = ne_concat(user_dir, "/", DAV_CONFIG, NULL);
    } else {
        args->conf = NULL;
    }

    args->user = 0;
    args->users = 0;
    args->netdev = 1;
    args->mopts = DAV_MOPTS;
    args->add_mopts = NULL;
    args->kernel_fs = NULL;
    args->buf_size = 0;

    args->uid = getuid();
    args->gid = getgid();
    args->dir_umask = 0;
    args->file_umask = 0;
    args->dir_mode = 0;
    args->file_mode = 0;

    args->scheme = NULL;
    args->host = NULL;
    args->port = 0;
    args->path = NULL;
    args->servercert = NULL;

    if (getuid() != 0) {
        args->secrets = ne_concat(user_dir, "/", DAV_SECRETS, NULL);
    } else {
        args->secrets = NULL;
    }
    args->username = NULL;
    args->cl_username = NULL;
    args->password = NULL;
    args->clicert = NULL;
    args->clicert_pw = NULL;

    args->p_host = NULL;
    args->p_port = DAV_DEFAULT_PROXY_PORT;
    args->p_user = NULL;
    args->p_passwd = NULL;
    args->useproxy = DAV_USE_PROXY;

    args->lock_owner = NULL;
    args->lock_timeout = DAV_LOCK_TIMEOUT;
    args->lock_refresh = DAV_LOCK_REFRESH;

    args->askauth = DAV_ASKAUTH;
    args->locks = DAV_LOCKS;
    args->expect100 = DAV_EXPECT100;
    args->if_match_bug = DAV_IF_MATCH_BUG;
    args->drop_weak_etags = DAV_DROP_WEAK_ETAGS;
    args->allow_cookie = DAV_ALLOW_COOKIE;
    args->precheck = DAV_PRECHECK;
    args->ignore_dav_header = DAV_IGNORE_DAV_HEADER;
    args->connect_timeout = DAV_CONNECT_TIMEOUT;
    args->read_timeout = DAV_READ_TIMEOUT;
    args->retry = DAV_RETRY;
    args->max_retry = DAV_MAX_RETRY;
    args->max_upload_attempts = DAV_MAX_UPLOAD_ATTEMPTS;
    args->s_charset = NULL;
    args->header = NULL;

    args->sys_cache = ne_strdup(DAV_SYS_CACHE);
    if (getuid() != 0) {
        args->cache_dir = ne_concat(user_dir, "/", DAV_CACHE, NULL);
    } else {
        args->cache_dir = NULL;
    }
    args->backup_dir = ne_strdup(DAV_BACKUP_DIR);
    args->cache_size = DAV_CACHE_SIZE;
    args->table_size = DAV_TABLE_SIZE;
    args->dir_refresh = DAV_DIR_REFRESH;
    args->file_refresh = DAV_FILE_REFRESH;
    args->delay_upload = DAV_DELAY_UPLOAD;
    args->gui_optimize = DAV_GUI_OPTIMIZE;

    args->debug = 0;
    args->neon_debug = 0;

    if (user_dir)
        free(user_dir);
    return args;
}


static void
log_dbg_cmdline(char *argv[])
{
    size_t len;
    char *cmdline;
    if (argz_create(argv, &cmdline, &len) == 0) {
        argz_stringify(cmdline, len, ' ');
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "%s", cmdline);
        free(cmdline);
    }
}


static void
log_dbg_config(char *argv[], dav_args *args)
{
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "Configuration:");
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  url: %s", url);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  mount point: %s", mpoint);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  dav_user: %s", args->dav_user);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  dav_group: %s", args->dav_group);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  conf: %s", args->conf);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  user: %i", args->user);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  netdev: %i", args->netdev);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  mopts: %#lx", args->mopts);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  add_mopts: %s", args->add_mopts);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  kernel_fs: %s", args->kernel_fs);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  buf_size: %i KiB", args->buf_size);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  uid: %i", args->uid);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  gid: %i", args->gid);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  dir_umask: %#o", args->dir_umask);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  file_umask: %#o", args->file_umask);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  dir_mode: %#o", args->dir_mode);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  file_mode: %#o", args->file_mode);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  scheme: %s", args->scheme);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  host: %s", args->host);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  port: %i", args->port);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  path: %s", args->path);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  servercert: %s", args->servercert);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  secrets: %s", args->secrets);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  clicert: %s", args->clicert);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  p_host: %s", args->p_host);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  p_port: %i", args->p_port);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  useproxy: %i", args->useproxy);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  askauth: %i", args->askauth);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  locks: %i", args->locks);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  lock_owner: %s", args->lock_owner);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  lock_timeout: %li s", args->lock_timeout);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  lock_refresh: %li s", args->lock_refresh);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  expect100: %i", args->expect100);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  if_match_bug: %i", args->if_match_bug);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  drop_weak_etags: %i", args->drop_weak_etags);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  allow_cookie: %i", args->allow_cookie);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  precheck: %i", args->precheck);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  ignore_dav_header: %i", args->ignore_dav_header);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  connect_timeout: %li s", args->connect_timeout);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  read_timeout: %li s", args->read_timeout);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  retry: %li s", args->retry);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  max_retry: %li s", args->max_retry);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  s_charset: %s", args->s_charset);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  header: %s", args->header);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  sys_cache: %s", args->sys_cache);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  cache_dir: %s", args->cache_dir);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  backup_dir: %s", args->backup_dir);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  cache_size: %i MiB", args->cache_size);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  table_size: %i", args->table_size);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  dir_refresh: %li s", args->dir_refresh);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  file_refresh: %li s", args->file_refresh);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  delay_upload: %i", args->delay_upload);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  gui_optimize: %i", args->gui_optimize);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  debug: %#x", args->debug);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG),
           "  neon_debug: %#x", args->neon_debug);
}


/* Parses line for max. parmc white-space separated parameter tokens and
   returns them in parmv[].
   The '#' character marks the beginning of a comment and the rest of the line
   is ignored.
   Parameters containing one of the characters ' ' (space), '\t' (tab), '\',
   '"' or '#' must be enclosed in double quotes '"' *or* this character has to
   be escaped by preceeding a '\'-character.
   Inside double quotes the '"'-character must be escaped. '\' may be escaped;
   it must be escaped if there is more than on '\'-character in succession.
   Whitespace characters other than ' ' and tab must only occur at the end of
   the line.
   line    : the line to be parsed. It will be changed by this function.
   parmc   : the max. number of parameters. It is an error, if more than parmc
             parameters are found
   parmv[] : the parameters found are returned in this array. It contains
             pointers into the rearranged line parameter.
   reurn value : the numer of parameters or -1 if an error occurs. */
static int
parse_line(char *line, int parmc, char *parmv[])
{
    enum {
        SPACE,
        SPACE_EXP,
        PARM,
        PARM_ESC,
        PARM_QUO,
        PARM_QUO_ESC,
        ERROR,
        END
    };

    int state = SPACE;
    int parm_no = 0;
    char *pos = line;
    char *p = line;
    parmv[0] = line;

    while (state != END) {
        switch (state) {
        case SPACE:
            if (*p == '\0' || *p == '#' || *p == '\f' || *p == '\n'
                    || *p == '\r' || *p == '\v') {
                state = END;
            } else if (*p == '\"') {
                state = PARM_QUO;
            } else if (*p == '\\') {
                state = PARM_ESC;
            } else if (isspace(*p)) {
                ;
            } else {
                *pos++ = *p;
                state = PARM;
            }
            if (parm_no >= parmc) return -1;
            break;
        case SPACE_EXP:
            if (*p == ' ' || *p == '\t') {
                state = SPACE;
            } else if (*p == '\0' || *p == '#' || *p == '\f' || *p == '\n'
                       || *p == '\r' || *p == '\v') {
                state = END;
            } else {
                return -1;
            }
            break;
        case PARM:
            if (*p == '\"') {
                return -1;
            } else if (*p == '\\') {
                state = PARM_ESC;
            } else if (*p == ' ' || *p == '\t') {
                *pos++ = '\0';
                parmv[++parm_no] = pos;
                state = SPACE;
            } else if (isspace(*p) || *p == '\0' || *p == '#') {
                *pos = '\0';
                parm_no++;
                state = END;
            } else {
                *pos++ = *p;
            }
            break;
        case PARM_ESC:
            if (*p == '\"' || *p == '\\' || *p == '#' || *p == ' '
                    || *p == '\t') {
                *pos++ = *p;
                state = PARM;
            } else {
                return -1;
            }
            break;
        case PARM_QUO:
            if (*p == '\\') {
                state = PARM_QUO_ESC;
            } else if (*p == '\"') {
                *pos++ = '\0';
                parmv[++parm_no] = pos;
                state = SPACE_EXP;
            } else if (*p == '\0' || *p == '\f' || *p == '\n'
                       || *p == '\r' || *p == '\v') {
                return -1;
            } else {
                *pos++ = *p;
            }
            break;
        case PARM_QUO_ESC:
            if (*p == '\"' || *p == '\\') {
                *pos++ = *p;
                state = PARM_QUO;
            } else if (*p == '\0' || *p == '\f' || *p == '\n'
                       || *p == '\r' || *p == '\v') {
                return -1;
            } else {
                *pos++ = '\\';
                *pos++ = *p;
                state = PARM_QUO;
            }
            break;
        }
        p++;
    }

    int i;
    for (i = parm_no; i < parmc; i++)
        parmv[i] = NULL;
    return parm_no;
}


/* Checks for a matching xxx_proxy environment variable, and if found
   stores values in args->p_host and ars->p_port. */
static void
proxy_from_env(dav_args *args)
{
    const char *env = NULL;
    if (args->scheme && strcmp(args->scheme, "https") == 0)
        env = getenv("https_proxy");
    if (!env)
        env = getenv("http_proxy");
    if (!env)
        env = getenv("all_proxy");
    if (!env)
        return;

    char *scheme = NULL;
    char *host = NULL;
    int port = 0;
    split_uri(&scheme, &host, &port, NULL, env);

    if (scheme && strcmp(scheme, "http") == 0 && host) {
        if (args->p_host) free(args->p_host);
        args->p_host = host;
        host = NULL;
        if (port)
            args->p_port = port;
    }

    if (scheme) free(scheme);
    if (host) free(host);
}


/* Reads the configuration file filename and stores the values in args.
   filename : name of the configuration file.
   system   : boolean value. 1 means it is the system wide configuration
              file. Some parameters are allowed only in the system wide
              configuration file, some only in the user configuration file. */
static void
read_config(dav_args *args, const char * filename, int system)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("opening %s failed"), filename);
        return;
    }

    size_t n = 0;
    char *line = NULL;
    int length = getline(&line, &n, file);
    int lineno = 1;
    int applies = 1;

    while (length > 0) {

        int parmc = 3;
        char *parmv[parmc];
        int count;
        count = parse_line(line, parmc, parmv);

        if (count == 1) {

            if (*parmv[0] != '[' || *(parmv[0] + strlen(parmv[0]) - 1) != ']')
                error_at_line(EXIT_FAILURE, 0, filename, lineno,
                              _("malformed line"));
            *(parmv[0] + strlen(parmv[0]) - 1) = '\0';
            applies = (strcmp(parmv[0] + 1, mpoint) == 0);

        } else if (applies && count == 2) {

            if (system && strcmp(parmv[0], "dav_user") == 0) {
                if (args->dav_user)
                    free(args->dav_user);
                args->dav_user = ne_strdup(parmv[1]); 
            } else if (system && strcmp(parmv[0], "dav_group") == 0) {
                if (args->dav_group)
                    free(args->dav_group);
                args->dav_group = ne_strdup(parmv[1]); 
            } else if (strcmp(parmv[0], "kernel_fs") == 0) {
                if (args->kernel_fs)
                    free(args->kernel_fs);
                args->kernel_fs = ne_strdup(parmv[1]); 
            } else if (strcmp(parmv[0], "buf_size") == 0) {
                args->buf_size = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "servercert") == 0) {
                if (args->servercert)
                    free(args->servercert);
                args->servercert = ne_strdup(parmv[1]);
            } else if (!system && strcmp(parmv[0], "secrets") == 0) {
                if (args->secrets)
                    free(args->secrets);
                args->secrets = ne_strdup(parmv[1]); 
            } else if (strcmp(parmv[0], "clientcert") == 0) {
                if (args->clicert)
                    free(args->clicert);
                args->clicert = ne_strdup(parmv[1]);
            } else if (system && strcmp(parmv[0], "proxy") == 0) {
                if (split_uri(NULL, &args->p_host, &args->p_port, NULL,
                              parmv[1]) != 0)
                    error_at_line(EXIT_FAILURE, 0, filename, lineno,
                                  _("malformed line"));
            } else if (system && strcmp(parmv[0], "use_proxy") == 0) {
                args->useproxy = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "ask_auth") == 0) {
                args->askauth = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "use_locks") == 0) {
                args->locks = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "lock_owner") == 0) {
                if (args->lock_owner)
                    free(args->lock_owner);
                args->lock_owner = ne_strdup(parmv[1]);
            } else if (strcmp(parmv[0], "lock_timeout") == 0) {
                args->lock_timeout = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "lock_refresh") == 0) {
                args->lock_refresh = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "use_expect100") == 0) {
                args->expect100 = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "if_match_bug") == 0) {
                args->if_match_bug = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "drop_weak_etags") == 0) {
                args->drop_weak_etags = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "allow_cookie") == 0) {
                args->allow_cookie = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "precheck") == 0) {
                args->precheck = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "ignore_dav_header") == 0) {
                args->ignore_dav_header = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "connect_timeout") == 0) {
                args->connect_timeout = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "read_timeout") == 0) {
                args->read_timeout = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "retry") == 0) {
                args->retry = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "max_retry") == 0) {
                args->max_retry = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "max_upload_attempts") == 0) {
                args->max_upload_attempts = arg_to_int(parmv[1], 10, parmv[0]);
            } else if (strcmp(parmv[0], "server_charset") == 0) {
                if (args->s_charset)
                    free(args->s_charset);
                args->s_charset = ne_strdup(parmv[1]);
            } else if (system && strcmp(parmv[0], "cache_dir") == 0) {
                if (args->sys_cache)
                    free(args->sys_cache);
                args->sys_cache = ne_strdup(parmv[1]); 
            } else if (!system && strcmp(parmv[0], "cache_dir") == 0) {
                if (args->cache_dir != NULL)
                    free(args->cache_dir);
                args->cache_dir = ne_strdup(parmv[1]); 
            } else if (strcmp(parmv[0], "backup_dir") == 0) {
                if (args->backup_dir)
                    free(args->backup_dir);
                args->backup_dir = ne_strdup(parmv[1]); 
            } else if (strcmp(parmv[0], "cache_size") == 0) {
                args->cache_size = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "table_size") == 0) {
                args->table_size = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "dir_refresh") == 0) {
                args->dir_refresh = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "file_refresh") == 0) {
                args->file_refresh = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "delay_upload") == 0) {
                args->delay_upload = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "gui_optimize") == 0) {
                args->gui_optimize = arg_to_int(parmv[1], 10, parmv[0]); 
            } else if (strcmp(parmv[0], "debug") == 0) {
                args->debug |= debug_opts(parmv[1]);
                args->neon_debug |= debug_opts_neon(parmv[1]);
            } else {
                error_at_line(EXIT_FAILURE, 0, filename, lineno,
                              _("unknown option"));
            }

        } else if (applies && count == 3) {

            if (strcmp(parmv[0], "add_header") == 0) {
                char *tmp = args->header;
                args->header = ne_concat(parmv[1], ": ", parmv[2], "\r\n", tmp,
                                         NULL);
                if (tmp)
                    free(tmp);
            } else {
                error_at_line(EXIT_FAILURE, 0, filename, lineno,
                              _("unknown option"));
            }

        } else if (count < 0 || count > 3) {

            error_at_line(EXIT_FAILURE, 0, filename, lineno,
                          _("malformed line"));
        }

        length = getline(&line, &n, file);
        lineno++;
    }

    if (line)
        free(line);
    fclose(file);
}

/* Reads environment variable no_proxy. no_proxy must be a comma separated
   list of domain names. If no_proxy is "*" or args->p_host matches any of 
   the entries in the no_proxy-list, args->p_host is removed. */
static void
read_no_proxy_list(dav_args *args)
{
    if (!args->p_host || !args->host)
        return;

    const char *env = getenv("no_proxy");
    if (!env)
        return;

    if (strcmp(env,"*") == 0) {
        free(args->p_host);
        args->p_host = NULL;
        return;
    }

    char *noproxy_list = ne_strdup(env);
    char *np = strtok(noproxy_list, ", ");
    while (np && args->p_host) {

        char *host = NULL;
        if (strchr(np, ':')) {
            if (asprintf(&host, "%s:%d", args->host, args->port) < 0)
                abort();
        } else {
            host = strdup(args->host);
        }

        if (*np == '.') {
            char *substr = strcasestr(host, np);
            if (substr && *(substr + strlen(np)) == '\0') {
                free(args->p_host);
                args->p_host = NULL;
            }
        } else {
            if (strcasecmp(host, np) == 0) {
                free(args->p_host);
                args->p_host = NULL;
            } 
        }

        free(host);
        np = strtok(NULL, ", ");
    }
    free(noproxy_list);
}


/* Searches the file filename for credentials for server url and for the proxy
   args->p_host and stores them in args. */
static void
read_secrets(dav_args *args, const char *filename)
{
    struct stat st;
    if (stat(filename, &st) < 0) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("opening %s failed"), filename);
        return;
    }
    if (st.st_uid != geteuid())
        error(EXIT_FAILURE, 0, _("file %s has wrong owner"), filename);
    if ((st.st_mode &
          (S_IXUSR | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX)) != 0)
        error(EXIT_FAILURE, 0, _("file %s has wrong permissions"), filename);

    FILE *file = fopen(filename, "r");
    if (!file) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("opening %s failed"), filename);
        return;
    }

    size_t n = 0;
    char *line = NULL;
    int length = getline(&line, &n, file);
    int lineno = 1;

    while (length > 0) {
        int parmc = 3;
        char *parmv[parmc];
        int count;
        count = parse_line(line, parmc, parmv);
        if (count != 0 && count != 3 && count != 2)
            error_at_line(EXIT_FAILURE, 0, filename, lineno,
                          _("malformed line"));

        if (count == 2 || count == 3) {

            char *scheme = NULL;
            char *host = NULL;
            int port = 0;
            char *path = 0;
            split_uri(&scheme, &host, &port, &path, parmv[0]);
            int p_port = port;
            if (scheme && !port)
                port = ne_uri_defaultport(scheme);

            char *ccert = NULL;
            if (args->clicert) {
                ccert = strrchr(args->clicert, '/');
                if (ccert && *(ccert + 1) == '\0')
                    ccert = NULL;
                if (ccert)
                    ccert++;
            }

            if ((mpoint && strcmp(parmv[0], mpoint) == 0)
                    || (mpoint && strstr(parmv[0], mpoint) == parmv[0]
                        && *(parmv[0] + strlen(mpoint)) == '/'
                        && *(parmv[0] + strlen(mpoint) + 1) == '\0')
                    || (scheme && args->scheme
                        && strcmp(scheme, args->scheme) == 0
                        && host && args->host && strcmp(host, args->host) == 0
                        && port == args->port
                        && path && args->path
                        && strcmp(path, args->path) == 0)) {

                if (args->username) {
                    memset(args->username, '\0', strlen(args->username));
                    free(args->username);
                }
                if (args->password) {
                    memset(args->password, '\0', strlen(args->password));
                    free(args->password);
                }
                args->username = ne_strdup(parmv[1]);
                if (count == 3)
                    args->password = ne_strdup(parmv[2]);

            } else if (strcmp(parmv[0], "proxy") == 0
                       || (host && args->p_host
                           && strcmp(host, args->p_host) == 0
                           && (!p_port || p_port == args->p_port))) {

                if (args->p_user) {
                    memset(args->p_user, '\0', strlen(args->p_user));
                    free(args->p_user);
                }
                if (args->p_passwd) {
                    memset(args->p_passwd, '\0', strlen(args->p_passwd));
                    free(args->p_passwd);
                }
                args->p_user = ne_strdup(parmv[1]);
                if (count == 3)
                    args->p_passwd = ne_strdup(parmv[2]);

            } else if (args->clicert
                       && (strcmp(parmv[0], args->clicert) == 0
                           || strcmp(parmv[0], ccert) == 0)) {

                if (count != 2)
                    error_at_line(EXIT_FAILURE, 0, filename, lineno,
                                  _("malformed line"));
                if (args->clicert_pw) {
                    memset(args->clicert_pw, '\0', strlen(args->clicert_pw));
                    free(args->clicert_pw);
                }
                args->clicert_pw = ne_strdup(parmv[1]);
            }

            if (scheme) free(scheme);
            if (host) free(host);
            if (path) free(path);
        }

        memset(line, '\0', strlen(line));
        length = getline(&line, &n, file);
        lineno++;
    }

    if (line) {
        memset(line, '\0', strlen(line));
        free(line);
    }
    fclose(file);
}


/* Splits an uri and returns the components.
   The uri must contain a host, the other components are optional. It must
   not contain userinfo. It shall not contain a query or fragment component;
   they would be treated as part of path.
   The path component must *not* be %-encoded. scheme, if present in uri,
   must be either http or https. If host is a IPv6 address, it must be enclosed
   in square brackets.
   The pointers to the components may be NULL. If they point to a non-NULL
   string, it is freed and then replaced by a newly allocated string.
   If no scheme is foud the default sheme "http" is returned.
   If no path is found "/" is returned as path. path will always end with "/".
   There is *no* default value returned for port.
   return value : 0 on success, -1 otherwise. */
static int
split_uri(char **scheme, char **host, int *port,char **path, const char *uri)
{
    if (!uri || !*uri) return -1;

    const char *sch = NULL;
    int po = 0;
    const char *ho = strstr(uri, "://");
    if (ho) {
        if ((ho - uri) == 4 && strcasestr(uri, "http") == uri) {
            sch = "http";
        } else if ((ho - uri) == 5 && strcasestr(uri, "https") == uri) {
            sch = "https";
        } else {
            return -1;
        }
        ho += 3;
    } else {
        ho = uri;
    }
    if (!*ho) return -1;

    const char *pa = strchrnul(ho, '/');
    if (pa == ho) return -1;

    const char *end = strchr(ho, '@');
    if (end && end < pa) return -1;

    if (*ho == '[') {
        end = strchr(ho, ']');
        if (!end || end >= pa) return -1;
        end++;
    } else {
        end = strchr(ho, ':');
        if (!end)
            end = pa;
    }

    if (end < pa) {
        if (end == ho || end == (pa - 1) || *end != ':') return -1;
        char *tail = NULL;
        po = strtol(end + 1, &tail, 10);
        if (po <= 0 || tail != pa) return -1;
    }

    if (scheme) {
        if (*scheme) free(*scheme);
        if (sch) {
            *scheme = strdup(sch);
        } else {
            *scheme = strdup("http");
        }
        if (!*scheme) abort();
    }

    if (port && po)
        *port = po;

    if (host) {
        if (*host) free(*host);
        *host = malloc(end - ho + 1);
        if (!*host) abort();
        int i;
        for (i = 0; i < (end - ho); i++) {
            if (*ho == '[') {
                *(*host + i) = islower(*(ho + i))
                               ? toupper(*(ho + i)) : *(ho + i);
            } else {
                *(*host + i) = isupper(*(ho + i))
                               ? tolower(*(ho + i)) : *(ho + i);
            }
        }
        *(*host + i) = '\0';
    }

    if (path) {
        if (*path) free(*path);
        if (!*pa) {
            *path = strdup("/");
        } else if (*(pa + strlen(pa) - 1) == '/') {
            *path = strdup(pa);
        } else {
            if (asprintf(path, "%s/", pa) < 1) abort();
        }
        if (!*path) abort();
    }

    return 0;
}


/* Prints version und help text. */
static void
usage(void)
{
    printf(_("Usage:\n"
             "    %s -V,--version   : print version string\n"
             "    %s -h,--help      : print this message\n\n"),
           PROGRAM_NAME, PROGRAM_NAME);
    printf(_("To mount a WebDAV-resource don't call %s directly, but use\n"
             "`mount' instead.\n"), PROGRAM_NAME);
    printf(_("    mount <mountpoint>  : or\n"
             "    mount <server-url>  : mount the WebDAV-resource as specified in\n"
             "                          /etc/fstab.\n"));
    printf(_("    mount -t davfs <server-url> <mountpoint> [-o options]\n"
             "                        : mount the WebDAV-resource <server-url>\n"
             "                          on mountpoint <mountpoint>. Only root\n"
             "                          is allowed to do this. options is a\n"
             "                          comma separated list of options.\n\n"));
    printf(_("Recognised options:\n"
             "    conf=        : absolute path of user configuration file\n"
             "    uid=         : owner of the filesystem (username or numeric id)\n"
             "    gid=         : group of the filesystem (group name or numeric id)\n"
             "    file_mode=   : default file mode (octal)\n"
             "    dir_mode=    : default directory mode (octal)\n"));
    printf(_("    ro           : mount read-only\n"
             "    rw           : mount read-write\n"
             "    [no]exec     : (don't) allow execution of binaries\n"
             "    [no]suid     : (don't) allow suid and sgid bits to take effect\n"
             "    [no]_netdev  : (no) network connection needed\n"));
}


/* Prints prompt to stdout and reads a line from stdin.
   A trailing newline is removed.
   return value : the user input. */
static char *
user_input(const char *prompt)
{
    printf("  %s ", prompt);
    char *line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, stdin);
    if (len < 0) abort();
    if (len > 0 && *(line + len - 1) == '\n')
        *(line + len - 1) = '\0';

    return line;
}


