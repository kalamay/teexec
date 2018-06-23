# teexec

`teexec` is a tool for the fail-safe duplication of inbound/rx TCP traffic
without modifying the application. The primary traffic going to the
original application is never held up by a slow secondary duplicated traffic.
That is, if the secondary system is too slow, it will be disconnected rather
than slow down the primary traffic.

## Example

```bash
$ make
$ ./build/bin/teexec -- nc -kl localhost 8080 # run in new shell
$ echo "test1" | nc localhost 8080 # message appears in first netcat
$ nc -U /tmp/teexec.sock # run in new shell
$ echo "test2" | nc localhost 8080 # message appears in two netcats
```

