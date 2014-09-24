#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.hpp"


char*
expandPath(const char* path) {
    char *s = NULL;
    if (path[0] == '~') {
        struct passwd *pw = getpwuid(getuid());
        const char *homedir = pw->pw_dir;
        myassert(0 < asprintf(&s, "%s%s", homedir, path+1));
    } else {
        s = strdup(path);
    }
    return s;
}

uint64_t
gettimeofdayMs(struct timeval* t)
{
    struct timeval now;
    if (NULL == t) {
        myassert(0 == gettimeofday(&now, NULL));
        t = &now;
    }
    return (((uint64_t)t->tv_sec) * 1000) + (uint64_t)floor(((double)t->tv_usec) / 1000);
}

void
printhex(const char *hdr,
         const unsigned char *md_value,
         unsigned int md_len)
{
    fprintf(stderr, "%s: printing hex: [", hdr);
    for(int i = 0; i < md_len; i++) {
        fprintf(stderr, "%02x", md_value[i]);
    }
    fprintf(stderr,"]\n");
}

void
to_hex(const unsigned char *value,
       unsigned int len,
       char *hex)
{
    for(int i = 0; i < len; i++) {
        sprintf(&(hex[i*2]), "%02x", value[i]);
    }
}

in_addr_t
getaddr(const char *hostname)
{
    if (!hostname || 0 == strlen(hostname)) {
        return htonl(INADDR_NONE);
    }
    /* check if we have an address as a string */
    struct in_addr in;
    int is_ip_address = inet_aton(hostname, &in);

    if(is_ip_address) {
        return in.s_addr;
    } else {
        in_addr_t addr = 0;

        /* get the address in network order */
        if(strcmp(hostname, "none") == 0) {
            addr = htonl(INADDR_NONE);
        } else if(strcmp(hostname, "localhost") == 0) {
            addr = htonl(INADDR_LOOPBACK);
        } else {
            struct addrinfo* info;
            int result = getaddrinfo(hostname, NULL, NULL, &info);
            if(result != 0) {
                myassert(0);
            }

            addr = ((struct sockaddr_in*)(info->ai_addr))->sin_addr.s_addr;
            freeaddrinfo(info);
        }

        return addr;
    }
}
