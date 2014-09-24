/* this file is simple: create the server socket, and on a new
 * connection, pass it to a newly launched Handler. it doesn't even
 * keep track of the connection or Handler.
 */


#include "webserver.hpp"
#include "common.hpp"
#include "handler.hpp"
#include "myassert.h"

#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>


extern ShadowLogFunc logfn;
extern ShadowCreateCallbackFunc scheduleCallback;

#ifdef ENABLE_MY_LOG_MACROS
/* "inst" stands for instance, as in, instance of a class */
#define loginst(level, inst, fmt, ...)                                  \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, webserver= %d): " fmt,  \
              __LINE__, (inst)->instNum_, ##__VA_ARGS__);               \
    } while (0)

/* like loginst, but shortcut "this" as instance */
#define logself(level, fmt, ...)                                        \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, webserver= %d): " fmt,  \
              __LINE__, (this)->instNum_, ##__VA_ARGS__);               \
    } while (0)

#else
/* no-ops */
#define loginst(level, inst, fmt, ...)

#define logself(level, fmt, ...)

#endif

uint32_t webserver_t::nextInstNum = 0;

static void
printUsageAndExit(const char* prog)
{
    logCRITICAL(
"USAGE: %s [listenport]\n"\
"          \n"\
"  listenport defaults to 80.\n"\
"", prog);
    exit(-1);
}

//namespace {
void
mev_readcb(int fd, void *ptr)
{
    int rv;
    webserver_t *s = (webserver_t *)(ptr);
    loginst(DEBUG, s, "begin");
    s->on_readable();
    loginst(DEBUG, s, "done");
}
//} // namespace

void
webserver_t::on_readable()
{
    static int numclients = 0;
    ++numclients;
    logself(DEBUG, "begin");
	gint sockd = accept(listenfd_, NULL, NULL);
	if(sockd < 0) {
		myassert(errno == EWOULDBLOCK);
	} else {
        /* instantiate and forget: handler will know to delete
         * itself */
        new Handler(evbase_, docroot_, sockd);
    }
    logself(DEBUG, "done");
}

void
webserver_t::start(int argc, char *argv[])
{
    int opt;
    int long_index;
    uint16_t listenport = 80;
    const char *listenip = "127.0.0.1";

#if 0
    struct option long_options[] = {

        {"listenip", required_argument, 0, 1001},

        {0, 0, 0, 0},
    };

    while ((opt = getopt_long(argc, argv, "", long_options, &long_index)) != -1)
    {
        switch (opt) {
        case 0:
            if (long_options[long_index].flag != 0) {
                break;
            }
            //qDebug() << "option " << long_options[long_index].name;
            if (optarg) {
                //qDebug() << " with arg " << optarg;
            }
            break;

        case 1001:
            listenip = optarg;
            myassert(0); // not yet supported
            break;

        default:
            myassert(0);
            break;
        }
    }

    if (optind >= argc) {
        logself(CRITICAL, "must specify listenport");
        myassert(0);
    }

    listenport = strtol(argv[optind], NULL, 10);

    if (optind == argc) {
        printUsageAndExit(argv[0]);
    }
#endif

    myassert(argc == 2 || argc == 3);

    char *expandedpath = expandPath(argv[1]);

    logself(DEBUG,
          "argv[1] = [%s], expandPath = [%s]", argv[1], expandedpath);

    docroot_ = expandedpath;
    free(expandedpath);

    logself(DEBUG, "docroot [%s]", docroot_.c_str());

    if (argc == 3) {
        listenport = strtol(argv[2], NULL, 10);
    }

    // it seems the log statement will be reported by valgrind as
    // "possibly lost"
    logself(DEBUG, "listen port [%d]", listenport);

	/* create the socket and get a socket descriptor */
	listenfd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	myassert (listenfd_ > 0);

    logself(DEBUG, "listenfd = %d", listenfd_);

	/* setup the socket address info, server will listen for incoming
	 * connections on listen_port
	 */
	struct sockaddr_in listener;
	memset(&listener, 0, sizeof(listener));
	listener.sin_family = AF_INET;
	listener.sin_addr.s_addr = htonl(INADDR_ANY);
	listener.sin_port = htons(listenport);

	/* bind the socket to the server port */
	int result = bind(
        listenfd_, (struct sockaddr *) &listener, sizeof(listener));
	myassert(!result);

	/* set as server listening socket */
	result = listen(listenfd_, 1000);
    myassert(!result);

    evbase_ = new myevent_base(logfn);
    myassert(evbase_);

    /* new cnx is notified as readable event */
    listenev_ = new myevent_socket_t(
        evbase_, listenfd_, mev_readcb, NULL, NULL, this);
    myassert(listenev_);
    listenev_->set_logfn(logfn);
    listenev_->set_connected(); // for now
    myassert(0 == listenev_->start_monitoring());

    logfn(SHADOW_LOG_LEVEL_MESSAGE, __func__,
          "webserver listening on port %u", listenport);

    return;
}

void webserver_free(webserver_t* b) {
    /* Clean up */

    // /* report stats */
    // if (b->state == SB_SUCCESS) {
    //  struct timespec duration_embedded_downloads;
    //  /* first byte statistics */
    //  duration_embedded_downloads.tv_sec = b->embedded_end_time.tv_sec - b->embedded_start_time.tv_sec;
    //  duration_embedded_downloads.tv_nsec = b->embedded_end_time.tv_nsec - b->embedded_start_time.tv_nsec;
        
    //  while(duration_embedded_downloads.tv_nsec < 0) {
    //      duration_embedded_downloads.tv_sec--;
    //      duration_embedded_downloads.tv_nsec += 1000000000;
    //  }
        
    //  b->shadowlib->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
    //      "Finished downloading %d/%d embedded objects (%zu bytes) in %lu.%.3d seconds, %d total bytes sent, %d total bytes received",
    //      b->embedded_downloads_completed,
    //      b->embedded_downloads_expected,
    //      b->cumulative_size - b->document_size,
    //      duration_embedded_downloads.tv_sec,
    //      (gint)(duration_embedded_downloads.tv_nsec / 1000000),
    //      b->bytes_uploaded,
    //      b->bytes_downloaded);
    // }
}

void
webserver_t::activate(const bool blocking)
{
    if (blocking) {
        evbase_->dispatch();
    } else {
        evbase_->loop_nonblock();
    }
}

webserver_t::~webserver_t()
{
    if (evbase_) {
        delete evbase_;
        evbase_ = NULL;
    }
}

webserver_t::webserver_t()
    : instNum_(nextInstNum), evbase_(NULL), listenev_(NULL), listenfd_(-1)
{
    ++nextInstNum;
}
