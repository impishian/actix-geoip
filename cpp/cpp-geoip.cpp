#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <maxminddb.h>
#include <string>
#include <optional>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>
#include <string_view>

using asio::ip::tcp;
using namespace asio;

struct GeoCity {
  std::string country_code;
  std::string country_name;
  std::string time_zone;
  double latitude = 0.0;
  double longitude = 0.0;
};

struct GeoIPReader {
  MMDB_s mmdb;

  GeoIPReader(const std::string &db_path) {
    int status = MMDB_open(db_path.c_str(), MMDB_MODE_MMAP, &mmdb);
    if (status != MMDB_SUCCESS) throw std::runtime_error(MMDB_strerror(status));
  }

  ~GeoIPReader() { MMDB_close(&mmdb); }

  std::optional<GeoCity> lookup(const std::string &ip) const {
    int gai_error, mmdb_error;
    MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, ip.c_str(), &gai_error, &mmdb_error);
    if (gai_error != 0 || mmdb_error != MMDB_SUCCESS || !result.found_entry) return std::nullopt;

    MMDB_entry_data_s data;
    GeoCity city;

    if (MMDB_get_value(&result.entry, &data, "country", "iso_code", NULL) == MMDB_SUCCESS && data.has_data)
      city.country_code = std::string(data.utf8_string, data.data_size);

    if (MMDB_get_value(&result.entry, &data, "country", "names", "en", NULL) == MMDB_SUCCESS && data.has_data)
      city.country_name = std::string(data.utf8_string, data.data_size);

    if (MMDB_get_value(&result.entry, &data, "location", "time_zone", NULL) == MMDB_SUCCESS && data.has_data)
      city.time_zone = std::string(data.utf8_string, data.data_size);

    if (MMDB_get_value(&result.entry, &data, "location", "latitude", NULL) == MMDB_SUCCESS && data.has_data)
      city.latitude = data.double_value;

    if (MMDB_get_value(&result.entry, &data, "location", "longitude", NULL) == MMDB_SUCCESS && data.has_data)
      city.longitude = data.double_value;

    return city;
  }
};

std::string build_geo_json(std::string_view ip, const std::optional<GeoCity> &geo) {
  const auto &g = geo.value_or(GeoCity{});
  return std::format(
        R"({{"ip":"{}","country_code":"{}","country_name":"{}","time_zone":"{}","latitude":{},"longitude":{}}})",
        ip, g.country_code, g.country_name, g.time_zone, g.latitude, g.longitude
    );
}

// Coroutine to handle a single connection
awaitable<void> session(tcp::socket sock, std::shared_ptr<GeoIPReader> reader) {
  try {
    char data[1024];  // Increase buffer size ?
    for (;;) {
      std::size_t n = co_await sock.async_read_some(buffer(data), use_awaitable);
      std::string_view req(data, n);

      auto pos = req.find("/json/");
      if (pos == std::string_view::npos) continue;

      auto end = req.find(' ', pos + 6);
      if (end == std::string_view::npos) continue;

      std::string ip(req.substr(pos + 6, end - pos - 6));
      std::string body = build_geo_json(ip, reader->lookup(ip));
      std::string resp = std::format("HTTP/1.1 200 OK\r\nContent-Length: {}\r\n"
                        "Content-Type: application/json\r\nConnection: keep-alive\r\n\r\n{}", body.size(), body);

      co_await async_write(sock, buffer(resp), use_awaitable);
    }
  } catch (...) {
    // Silently handle connection exceptions, which are normal network behavior
  }
}

// Coroutine accept loop 
awaitable<void> acceptor_loop(tcp::acceptor &acceptor, std::shared_ptr<GeoIPReader> reader) {
  for (;;) {
    tcp::socket sock = co_await acceptor.async_accept(use_awaitable);
    sock.set_option(tcp::no_delay(true));  // Reduce latency
    co_spawn(acceptor.get_executor(), session(std::move(sock), reader), detached);
  }
}

int main() {
  try {
    auto reader = std::make_shared<GeoIPReader>("/etc/maxminddb/GeoLite2-City.mmdb");

    asio::io_context io_ctx(std::thread::hardware_concurrency());
    tcp::acceptor acceptor(io_ctx, tcp::endpoint(tcp::v4(), 8888));
    acceptor.set_option(tcp::acceptor::reuse_address(true));

#ifdef SO_REUSEPORT
    int native_sock = acceptor.native_handle();
    int optval = 1;
    if (setsockopt(native_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0)
      perror("setsockopt(SO_REUSEPORT)");
#endif

    co_spawn(io_ctx, acceptor_loop(acceptor, reader), detached);

    std::vector<std::thread> threads;
    for (int i = 0; i < std::thread::hardware_concurrency(); ++i) threads.emplace_back([&] { io_ctx.run(); });
    for (auto &t : threads) t.join();
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << "\n";
  }
}
