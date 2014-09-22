#ifndef HTTP_PARSE_HPP
#define HTTP_PARSE_HPP

/* assumes that the trailing \r\n are not in buf */
int
parseRange(const char *buf, int i,
           int *from_return, int *to_return);
int
parseContentRange(const char *buf, int i,
                  int *from_return, int *to_return, int *full_len_return);

#endif /* HTTP_PARSE_HPP */
