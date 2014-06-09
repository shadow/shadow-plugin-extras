# "browser": a Shadow plug-in

This plug-in tries to immitate the behaviour of a modern browser: it not only downloads an HTML document (which is already possible with the [filetransfer plug-in](https://github.com/shadow/shadow-plugin-extras/tree/master/filetransfer)), but also parses the HTML and downloads the following types of embedded objects:

+ favorite icons
+ stylesheets (embedded with a link-tag)
+ images (embedded with an img-tag)

## copyright holders

caffeineshock

## licensing deviations

None specified.

## last known working version

This plug-in was last tested and known to work with 
~~Shadow x.y.z(-dev) commit <commit hash>~~
unknown, but definitely <= 1.8.0.

## usage

### example

Simulate a small test website by running `shadow example.xml` from the `browser/` directory.

### wikipedia

To simulate browser requests to www.wikipedia.org the site first has to be mirrored to the local filesystem:

```bash
mkdir some-directory
cd some-directory
wget -H -p http://www.wikipedia.org
cd ../
```

This will create directories for each hostname (namely `bits.wikimedia.org`,`en.wikipedia.org`, `upload.wikimedia.org` and `www.wikipedia.org`) and download the HTML document and the embedded resources to the respective directory.

This has been done and the contents are given in the `browser/wiki` directory. You can run the wikipedia example with `shadow wiki.xml`.

### browser program args

The arguments for the browser plugin denote the following:

1. HTTP server address/name
2. HTTP server port
3. SOCKS proxy address/name
4. SOCKS proxy port
5. Maximum amount of concurrent connections per host
6. Path of the HTML document

### browser output

Run an experiment like so:

```
shadow example.xml | grep browser_
```

The output should be like the following:

```
[browser_launch] Trying to simulate browser access to /index.html on www.wikipedia.org
[browser_downloaded_document] first document (46166 bytes) downloaded and parsed in 1.176 seconds, now getting 16 additional objects...
[browser_free] Finished downloading 16/16 embedded objects (28376 bytes) in 1.395 seconds
```

## implementation

Like a browser, the plugin opens multiple persistent HTTP connections per host. The maximum of concurrent connections per host can be limited though (which all modern browser do [as well](http://www.browserscope.org/?category=network)). When there are more downloads than connections available the connections are reused once a download finishes.
