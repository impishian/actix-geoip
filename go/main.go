package main

import (
    "encoding/json"
    "flag"
    "fmt"
    "log"
    "net"
    "net/http"

    "github.com/julienschmidt/httprouter"
    "github.com/oschwald/geoip2-golang"
)

type responseRecord struct {
    IP          string  `json:"ip"`
    CountryCode string  `json:"country_code"`
    CountryName string  `json:"country_name"`
   // RegionCode  string  `json:"region_code"`
   // RegionName  string  `json:"region_name"`
   // City        string  `json:"city"`
   // ZipCode     string  `json:"zip_code"`
    TimeZone    string  `json:"time_zone"`
    Latitude    float64 `json:"latitude"`
    Longitude   float64 `json:"longitude"`
   // MetroCode   uint    `json:"metro_code"`
}

func Index(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
    fmt.Fprint(w, "Usage: curl localhost:9999/json/1.2.3.4\n\nGeoLite2-City.mmdb is from https://github.com/P3TERX/GeoLite.mmdb/raw/download/GeoLite2-City.mmdb!\n\n")
}

func QueryIP(w http.ResponseWriter, r *http.Request, params httprouter.Params) {
    var rr responseRecord

    ip := params.ByName("ip")
    ipv4 := net.ParseIP(ip)

    if ipv4 == nil {
        w.WriteHeader(http.StatusNotFound)
        w.Write([]byte("404 page not found"))
        return
    }

    record, err := db.City(ipv4)
    if err != nil {
        w.WriteHeader(http.StatusNotFound)
        w.Write([]byte("404 page not found"))
        return
    }

    rr.IP = ip
    rr.CountryCode = record.Country.IsoCode
    rr.CountryName = record.Country.Names["en"]
    //rr.City = record.City.Names["en"]
    //rr.ZipCode = record.Postal.Code
    rr.TimeZone = record.Location.TimeZone
    rr.Latitude = record.Location.Latitude
    rr.Longitude = record.Location.Longitude

    // example1: {"ip":"1.2.3.4","country_code":"RU","country_name":"Russia","region_code":"MOW","region_name":"Moscow","city":"Moscow","zip_code":"105179","time_zone":"Europe/Moscow","latitude":55.7527,"longitude":37.6172,"metro_code":0}
    // example2: {"ip":"123.223.33.42","country_code":"JP","country_name":"Japan","region_code":"","region_name":"","city":"","zip_code":"","time_zone":"Asia/Tokyo","latitude":35.69,"longitude":139.69,"metro_code":0}
    //if len(record.Subdivisions) > 0 {
    //    rr.RegionCode = record.Subdivisions[0].IsoCode
    //    rr.RegionName = record.Subdivisions[0].Names["en"]
    //}

    if err := json.NewEncoder(w).Encode(rr); err != nil {
        panic(err)
    }
}

var db *geoip2.Reader
var err error

// To Compile:
// [Linux]       CGO_ENABLED=0 go build mygeoip.go
// or:
// [Cross compile on macOS] CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o ../bin/freegeoip ./main.go

// To Run:
// ./freegeoip -f /etc/maxminddb/GeoLite2-City.mmdb

func main() {
    configFile := flag.String("f", "", "GeoLite2-City.mmdb file path")
    flag.Parse()

    db, err = geoip2.Open(*configFile)
    if err != nil {
        fmt.Println("GeoLite2-City.mmdb not found. Please get it from https://github.com/P3TERX/GeoLite.mmdb ")
        log.Fatal(err)
    }
    defer db.Close()

    router := httprouter.New()
    router.GET("/", Index)
    router.GET("/json/:ip", QueryIP)

    log.Fatal(http.ListenAndServe(":8888", router))
}
