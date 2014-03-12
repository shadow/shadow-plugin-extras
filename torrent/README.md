# "xxxxx": a Shadow plug-in

The `torrent` plug-in allows configuration of a BitTorrent-like P2P swarm to share files.

## copyright holders

John Geddes

## licensing deviations

Unknown

## last known working version

This plug-in was last tested and known to work with 
~~Shadow x.y.z(-dev) commit <commit hash>~~
unknown, but definitely <= Shadow v1.8.0.

## usage

**NOTE: the torrent plug-in currently [contains bugs](https://github.com/shadow/shadow/issues/125) and is not expected to work correctly. If you run into problems, please skip this section.**

The `torrent` plug-in allows configuration of a BitTorrent-like P2P swarm to share an 8 MiB file between all of 10 clients. A torrent 'authority' represents a tracker and assists clients in joining the swarm, and the file is shared in 16 KiB chunks. This example will also take a few minutes, and redirecting output is advised:

```bash
shadow --torrent > torrenttest.log
```

Useful statistics here are contained in messages labeled with `client-block-complete`, which is printed for each node upon the completion of each 16 KiB block, and `client-complete`, which is printed when the transfer finishes. An 8 MiB file should contain 512 blocks, so for all 10 clients there should be **5120** blocks total:

```bash
grep "client-block-complete" torrenttest.log | wc -l
```

And all **10** clients should have completed:

```bash
grep "client-complete" torrenttest.log | wc -l
```
