/* copied from polipo, with a few changes such as removed "restrict"
 * keyword and "digit()" -> "isdigit()".
 */

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

static int
skipComment(const char *buf, int i)
{
    assert(buf[i] == '(');
    i++;
    while(1) {
        if(buf[i] == '\\' && buf[i + 1] == ')') i+=2;
        else if(buf[i] == ')') return i + 1;
        else if(buf[i] == '\n') {
            if(buf[i + 1] == ' ' || buf[i + 1] == '\t')
                i += 2;
            else
                return -1;
        } else if(buf[i] == '\r') {
            if(buf[i + 1] != '\n') return -1;
            if(buf[i + 2] == ' ' || buf[i + 2] == '\t')
                i += 3;
            else
                return -1;
        } else {
            i++;
        }
    }
    return i;
}

static int
skipWhitespace(const char *buf, int i)
{
    while(1) {
        if(buf[i] == ' ' || buf[i] == '\t')
            i++;
        else if(buf[i] == '(') {
            i = skipComment(buf, i);
            if(i < 0) return -1;
        } else if(buf[i] == '\n') {
            if(buf[i + 1] == ' ' || buf[i + 1] == '\t')
                i += 2;
            else
                return i;
        } else if(buf[i] == '\r' && buf[i + 1] == '\n') {
            if(buf[i + 2] == ' ' || buf[i + 2] == '\t')
                i += 3;
            else
                return i;
        } else
            return i;
    }
}

static int
skipEol(const char *buf, int i)
{
    while(buf[i] == ' ')
        i++;
    if(buf[i] == '\n')
        return i + 1;
    else if(buf[i] == '\r') {
        if(buf[i + 1] == '\n')
            return i + 2;
        else
            return -1;
    } else {
        return -1;
    }
}

static int
parseInt(const char *buf, int start, int *val_return)
{
    int i = start, val = 0;
    if(!isdigit(buf[i]))
        return -1;
    while(isdigit(buf[i])) {
        val = val * 10 + (buf[i] - '0');
        i++;
    }
    *val_return = val;
    return i;
}

int
parseRange(const char *buf, int i,
           int *from_return, int *to_return)
{
    int j;
    int from, to;
    i = skipWhitespace(buf, i);
    if(i < 0)
        return -1;
    if(strncmp(buf + i, "bytes=", 6))
        return -1;
    i += 6;
    i = skipWhitespace(buf, i);
    if(buf[i] == '-') {
        from = 0;
    } else {
        i = parseInt(buf, i, &from);
        if(i < 0) return -1;
    }
    if(buf[i] != '-')
        return -1;
    i++;
    j = parseInt(buf, i, &to);
    if(j < 0)
        to = -1;
    else {
#if 0
        /* why add 1 to the last-byte-pos? */
        to = to + 1;
#endif
        i = j;
    }
#if 0
    j = skipEol(buf, i);
    if(j < 0) return -1;
#endif
    *from_return = from;
    *to_return = to;
    return i;
}

int
parseContentRange(const char *buf, int i,
                  int *from_return, int *to_return, int *full_len_return)
{
    int j;
    int from, to, full_len;
    i = skipWhitespace(buf, i);
    if(i < 0) return -1;
    if(strncmp(buf + i, "bytes", 5)) {
        return -1;
    } else {
        i += 5;
    }
    i = skipWhitespace(buf, i);
    if(buf[i] == '*') {
        from = 0;
        to = -1;
        i++;
    } else {
        i = parseInt(buf, i, &from);
        if(i < 0) return -1;
        if(buf[i] != '-') return -1;
        i++;
        i = parseInt(buf, i, &to);
        if(i < 0) return -1;
#if 0
        /* why add 1 to the last-byte-pos? */
        to = to + 1;
#endif
    }
    if(buf[i] != '/')
        return -1;
    i++;
    if(buf[i] == '*')
        full_len = -1;
    else {
        i = parseInt(buf, i, &full_len);
        if(i < 0) return -1;
    }
#if 0
    j = skipEol(buf, i);
    if(j < 0)
        return -1;
#endif
    *from_return = from;
    *to_return = to;
    *full_len_return = full_len;
    return i;
}
