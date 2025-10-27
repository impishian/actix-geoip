# actix-geoip
Ultra-light, zero-allocation GeoIP lookup micro-service built on Actix-web.

## What it does

GET /json/{ip}

returns instant, self-contained JSON with the caller’s **country code, country name, time-zone, latitude & longitude** (plus the queried IP) using the MaxMind GeoLite2-City database.

No serde, no templates, no heap churn per request – the response is streamed straight from a pre-allocated BytesMut buffer.

```bash
# 1. Download GeoLite2-City.mmdb and place it at /etc/maxminddb/GeoLite2-City.mmdb
# 2. cargo run --release
# 3. curl http://localhost:8888/json/8.8.8.8
```

Response:

```json
{"ip":"8.8.8.8","country_code":"US","country_name":"United States","time_zone":"America/Chicago","latitude":"37.751","longitude":"-97.822"}
```

## Environment

- ACTIX_WORKERS – number of worker threads (defaults to CPU count)
- Listens on 0.0.0.0:8888 (customize via BIND_ADDR constant).

## Build & Run

```bash
cargo build --release
sudo ./target/release/actix-geoip   # needs read access to /etc/maxminddb/GeoLite2-City.mmdb
```

Repository includes nothing but main.rs and Cargo.toml – drop it into any container or VM and you have a ~5 MB GeoIP endpoint.

## wrk


### rust:

```bash
$ wrk -c 100 -d 10 -t 20 http://192.168.168.168:8888/json/1.2.3.4

Running 10s test @ http://192.168.168.168:8888/json/1.2.3.4
  20 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   440.56us  169.84us   3.57ms   72.02%
    Req/Sec    11.15k     0.97k   15.06k    69.52%
  2239852 requests in 10.10s, 459.26MB read
Requests/sec: 221765.55
Transfer/sec:     45.47MB
```

### cpp:

```bash
$ wrk -c 100 -d 10 -t 20 http://192.168.168.168:8888/json/1.2.3.4
Running 10s test @ http://192.168.168.168:8888/json/1.2.3.4
  20 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   394.92us  207.32us   8.87ms   90.79%
    Req/Sec    12.52k   771.73    17.09k    80.08%
  2513334 requests in 10.10s, 551.29MB read
Requests/sec: 248848.79
Transfer/sec:     54.58MB
```

### go:

```bash
$ wrk -c 100 -d 10 -t 20 http://192.168.168.168:8888/json/1.2.3.4
Running 10s test @ http://192.168.168.168:8888/json/1.2.3.4
  20 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     0.99ms  724.91us   8.56ms   74.66%
    Req/Sec     5.50k   203.76     6.73k    68.48%
  1103741 requests in 10.10s, 266.31MB read
Requests/sec: 109285.27
Transfer/sec:     26.37MB
```
