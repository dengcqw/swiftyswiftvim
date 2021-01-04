#import "Logging.hpp"
#include "boost/asio/strand.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/bind/bind.hpp"
#include "boost/asio/placeholders.hpp"
#import <boost/beast.hpp>
#import <boost/asio.hpp>
#import <cstddef>
#import <cstdio>
#import <iostream>
#import <memory>
#import <mutex>
#import <sstream>
#import <thread>
#import <utility>

namespace ssvim {
namespace http {

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

struct ServiceContext {
public:
  const std::string secret;
  const LogLevel logLevel;
  ServiceContext(std::string secret, LogLevel logLevel)
      : secret(secret), logLevel(logLevel) {
  }
};

/**
 * SSVI HTTP Server is a HTTP front end for Swift Semantic
 * tasks.
 */
class SemanticHTTPServer: public std::enable_shared_from_this<SemanticHTTPServer> {
  using endpoint_type = net::ip::tcp::endpoint;
  using address_type = net::ip::address;
  using socket_type = net::ip::tcp::socket;

  std::mutex _sharedMutex;
  net::io_context& ioc_;
  net::ip::tcp::acceptor _acceptor;
  std::string _root_path;
  ServiceContext _context;

public:
  SemanticHTTPServer(
    net::io_context& ioc,
    endpoint_type const &ep,
    std::string const &root,
    ServiceContext const &context)
    : ioc_(ioc),
    _acceptor(net::make_strand(ioc)),
    _root_path(root),
    _context(context)
  {

    _acceptor.open(ep.protocol());
    _acceptor.bind(ep);
    _acceptor.listen();
  }


  ~SemanticHTTPServer() {}
  void run();

private:
  void doAccept();
  void onAccept(beast::error_code ec, socket_type socket);
};

} // namespace http
} // namespace ssvim
