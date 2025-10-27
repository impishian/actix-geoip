use actix_web::{
    get, web::{Data, Path}, App, HttpServer, HttpResponse, Responder, Result,
};
use maxminddb::geoip2;
use std::{net::IpAddr, sync::Arc};
use bytes::BytesMut;

const MMDB_PATH: &str = "/etc/maxminddb/GeoLite2-City.mmdb";
const BIND_ADDR: &str = "0.0.0.0:8888";

// Generate JSON directly into the buffer
fn build_geo_json(ip: &IpAddr, city: Option<&geoip2::City>) -> BytesMut {
    let mut buf = BytesMut::with_capacity(256);

    buf.extend_from_slice(b"{\"ip\":\"");
    buf.extend_from_slice(ip.to_string().as_bytes());
    buf.extend_from_slice(b"\",\"country_code\":\"");
    buf.extend_from_slice(city
        .and_then(|c| c.country.as_ref())
        .and_then(|c| c.iso_code)
        .unwrap_or("")
        .as_bytes());
    buf.extend_from_slice(b"\",\"country_name\":\"");
    buf.extend_from_slice(city
        .and_then(|c| c.country.as_ref())
        .and_then(|c| c.names.as_ref())
        .and_then(|n| n.get("en"))
        .unwrap_or(&"")
        .as_bytes());
    buf.extend_from_slice(b"\",\"time_zone\":\"");
    buf.extend_from_slice(city
        .and_then(|c| c.location.as_ref())
        .and_then(|l| l.time_zone)
        .unwrap_or("")
        .as_bytes());
    buf.extend_from_slice(b"\",\"latitude\":\"");
    buf.extend_from_slice(city
        .and_then(|c| c.location.as_ref())
        .and_then(|l| l.latitude)
        .map(|v| v.to_string())
        .unwrap_or_default()
        .as_bytes());
    buf.extend_from_slice(b"\",\"longitude\":\"");
    buf.extend_from_slice(city
        .and_then(|c| c.location.as_ref())
        .and_then(|l| l.longitude)
        .map(|v| v.to_string())
        .unwrap_or_default()
        .as_bytes());
    buf.extend_from_slice(b"\"}");

    buf
}

#[get("/json/{ip}")]
async fn handler_lookup(
    path: Path<String>,
    reader: Data<Arc<maxminddb::Reader<Vec<u8>>>>,
) -> Result<impl Responder> {
    /*let ip: IpAddr = path.into_inner().parse().map_err(|_| {
        HttpResponse::BadRequest()
        .content_type("application/json")
        .body(r#"{"error":"invalid ip"}"#)
    })?;*/
    let ip = match path.into_inner().parse::<IpAddr>() {
        Ok(ip) => ip,
        Err(_) => return Ok(HttpResponse::BadRequest()
          .content_type("application/json")
          .body(r#"{"error":"invalid ip"}"#)),
    };

    if ip.is_loopback() {
        return Ok(HttpResponse::Ok().body(build_geo_json(&ip, None)));
    }

    let city = reader.lookup::<geoip2::City>(ip).ok();
    let json_buf = build_geo_json(&ip, city.as_ref());

    Ok(HttpResponse::Ok().body(json_buf))
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    std::panic::set_hook(Box::new(|info| {
        if let Some(s) = info.payload().downcast_ref::<&str>() {
            eprintln!("panic: {}", s);
        } else {
            eprintln!("panic occurred");
        }
    }));
    let reader = maxminddb::Reader::open_readfile(MMDB_PATH)
        .expect("Failed to open MMDB file");
    let reader = Arc::new(reader);

    let workers = std::env::var("ACTIX_WORKERS")
        .ok()
        .and_then(|w| w.parse().ok())
        .unwrap_or_else(|| num_cpus::get());

    println!("Listening on http://{} with {} workers", BIND_ADDR, workers);

    HttpServer::new(move || {
        App::new()
            .app_data(Data::new(reader.clone()))
            .service(handler_lookup)
    })
    .workers(workers)
    .bind(BIND_ADDR)?
    .run()
    .await
}
