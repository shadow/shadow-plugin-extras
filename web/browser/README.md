# "browser": a Shadow plug-in

This plug-in tries to immitate the behaviour of a modern browser: it not only downloads an HTML document, but also parses the HTML and downloads the following types of embedded objects:

+ images (embedded with an img-tag)
+ javascripts (embedded with a script tag)

Simulated JavaScript processing enables modeling web page object dependencies.

## copyright holders

Giang Nguyen (nguyen59@illinois.edu)

## licensing deviations

None specified.

## last known working version

This plug-in was last tested and known to work with Shadow 1.9.2.

## usage

### example

See examples directory. There are two examples: `with-tor`, which requires the scalion plugin, and `no-tor`. Both require the `webserver` plugin.

### browser program args

The arguments for the browser plugin denote the following:

USAGE: `--socks5 <host:port>|none --max-persist-cnx-per-srv ...|none --page-spec <path> --think-times <path>|none --timeoutSecs <path>|none --mode-spec <path>|none`

  * `--mode-spec`: a file that specifies each client's mode, vanilla or spdy (SPDY mode is not yet complete).
    USE `none` at this time, and the browser defaults to vanilla (HTTP).
  * page spec contains specification of multiple pages to load: each page
    spec begins with a line `page-url: <url>`, and the following lines should
    be `objectURL | objectSize | (optional) objectMd5Sum`
    empty lines or lines beginning with # are ignored.
    see the examples directory for an example page spec, with a multi-resource page and a file.
  * if `--think-times` is `none`, then no think times between downloads; if it's
    a number N > 1, then it's considered the upperbound of a uniform range
    [1, N] millieconds; otherwise, it's assumed to be a path to a cdf file.
  * `--timeoutSecs`: how long (seconds) before a page/file load is reported as failed.

### browser output

Successful loads look like:

```
[report_result] loadnum= 2, vanilla: success: start= 28328 plt= 168 url= [http://server-3/one-file.bin] ttfb= 168 totalbodybytes= 12345 totaltxbytes= 80 totalrxbytes= 12446 numobjects= 1 numerrorobjects= 0
[report_result] loadnum= 7, vanilla: success: start= 189154 plt= 564 url= [http://server-2/index.html] ttfb= 156 totalbodybytes= 82125 totaltxbytes= 316 totalrxbytes= 82807 numobjects= 7 numerrorobjects= 0
```

All time fields are in milliseconds unless otherwise specified:

   * `start`: time when the load started.
   * `plt`: "page load time," the elapsed time to complete the page/file download.
   * `ttfb`: the elapsed time to receive the first byte of the page/file download.
   * `totaltxbytes`: total number of bytes sent.
   * `totalbodybytes`: total number of bytes of the response bodies.
   * `totalrxbytes`: total number of bytes received, including meta/control info such as headers.
   * `numobjects`: total number of resources downloaded during this page/file load.
   * `numerrorobjects`: total number of problematic resources downloaded during this page/file load.

A failed load due to digest mismatch(s) looks like:

```
[validate_one_resource] error: resource [http://server-2/2.jpg] expected digest= 397a954c5c507621521f3108612441aF, actual= 397a954c5c507621521f3108612441af
[report_result] loadnum= 7, vanilla: FAILED: start= 189154 plt= 0 url= [http://server-2/index.html] ttfb= 0 totalbodybytes= 82125 totaltxbytes= 316 totalrxbytes= 82807 numobjects= 7 numerrorobjects= 1
```

A filed load due to timeouts looks like:

```
[report_failed_load] loadnum= 160, vanilla: FAILED: start= 841181 reason= [timedout] url= [http://server-2/index.html] totalrxbytes= 41317
```

## implementation

This browser plugin uses a connection manager that opens and maintains multiple persistent HTTP connections per server host. The browser (class) does not deal directly with connections but only submits requests to the connection manager. The connection manager handles queuing of the requests and submitting them to the managed connections. The connections notify the browser using callbacks (via the requests) as the response bytes flow in. If a connection fails, the connection manager tries to re-request the affected resources on other new/existing connections, asking for only the missing byte ranges.

### Dependency-via-JavaScript support format

The browser interprets each line in the script body, either an external script (must have `.js` file extension) or inline script, that has this format:

```
// delayed_load: url= <url> delayms= <delay>
```

as an instruction to schedule the download of `<url>` (`<delay>` is currently ignored and `0` is used). The `<url>` can be another script, which will be similarly processed, thus enabling arbitrary dependency depths.
