#import "SemanticHTTPServer.hpp"
#include "boost/core/ignore_unused.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/beast/http/write.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/asio/placeholders.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/asio/streambuf.hpp"
#import "Logging.hpp"
#import "SwiftCompleter.hpp"

#import <boost/beast.hpp>
#import <boost/asio.hpp>
#import <boost/property_tree/json_parser.hpp>
#import <boost/property_tree/ptree.hpp>

#import <dispatch/dispatch.h>

#import <cstddef>
#import <cstdio>
#import <functional>
#import <iostream>
#import <map>
#import <memory>
#import <mutex>
#import <sstream>
#import <string>
#import <thread>
#import <utility>

using namespace ssvim;

namespace ssvim {
namespace http {

namespace http = beast::http;       // from <boost/beast/http.hpp>
using socket_type = net::ip::tcp::socket;
using req_type = http::request<http::string_body>;
using resp_type = http::response<http::string_body>;

static auto HeaderValueContentTypeJSON = "application/json";
static auto HeaderKeyContentType = http::field::content_type;
static auto HeaderKeyServer = http::field::server;
static auto HeaderValueServer = "SSVIM";

/**
 * Session is an instance of an HTTP Session.
 *
 * The server will allocate a new instance for each accepted
 * request.
 */
class Session;

using EndpointFn = std::function<void(std::shared_ptr<Session>)>;

class EndpointImpl : public std::enable_shared_from_this<EndpointImpl> {
  EndpointFn _start;

public:
  EndpointImpl(EndpointFn start);
  void handleRequest(std::shared_ptr<Session> session);
};

using namespace ssvim;

EndpointImpl makeSlowTestEndpoint();
EndpointImpl makeStatusEndpoint();
EndpointImpl makeShutdownEndpoint();
EndpointImpl makeCompletionsEndpoint();
EndpointImpl makeDiagnosticsEndpoint();

resp_type notFoundResponse(req_type request);
resp_type errorResponse(req_type request, std::string message);

class Session : public std::enable_shared_from_this<Session> {
  net::streambuf _streambuf;
  beast::tcp_stream _socket;
  ServiceContext _context;
  req_type _request;
  std::map<std::string, EndpointImpl> _endpoints;
  EndpointImpl *_endpoint;
  Logger _logger;

public:
  Session &operator=(Session &&) = delete;
  Session &operator=(Session const &) = delete;

  Session(socket_type &&sock, ServiceContext ctx) : _socket(std::move(sock)), _context(ctx), _logger(ctx.logLevel, "HTTP") {
    _endpoint = NULL;
    _logger.log(LogLevelInfo, "Secret:", _context.secret);
    // Setup Endpoints.
    // TODO: Perhaps this can be done statically
    _endpoints = std::map<std::string, EndpointImpl>();
    auto insert_endpoint = [&](std::string named, EndpointImpl impl) {
      _endpoints.insert(std::pair<std::string, EndpointImpl>(named, impl));
    };

    insert_endpoint("/status", makeStatusEndpoint());
    insert_endpoint("/shutdown", makeShutdownEndpoint());
    insert_endpoint("/completions", makeCompletionsEndpoint());
    insert_endpoint("/diagnostics", makeDiagnosticsEndpoint());
    insert_endpoint("/slow_test", makeSlowTestEndpoint());
  }

public:
  void start() {
    net::dispatch(
        _socket.get_executor(),
        beast::bind_front_handler(
            &Session::doRead,
            this->shared_from_this()));
  }

  Logger logger() {
    return _logger;
  }

  std::shared_ptr<Session> detach() {
    return shared_from_this();
  }

  void doClose() {
      beast::error_code ec;
      _socket.socket().shutdown(tcp::socket::shutdown_send, ec);
  }

  void doRead() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    _request = {};

    // Set the timeout.
    _socket.expires_after(std::chrono::seconds(30));

    http::async_read(_socket, _streambuf, _request,
      boost::bind(&Session::onRead, shared_from_this(), net::placeholders::error, net::placeholders::bytes_transferred)
    );
  }

  void onRead(beast::error_code ec, std::size_t bytes_transferred) {
    _logger << "ONREAD";
    boost::ignore_unused(bytes_transferred);

    if (ec == http::error::end_of_stream) {
        return doClose();
    }

    if (ec)
      return fail(ec, "read");

    auto path = _request.target();

    // Typical flow of handling a response
    // - Detach and retain - necessary to keep this alive.
    // - Quickly return to prevent from blocking acceptor loop.
    // - Perform a long running task.
    // - Schedule write for the response body
    auto detachedSession = detach();

    auto endpointImpl = _endpoints.find(std::string(path));
    if (endpointImpl != _endpoints.end()) {
      _logger << "GOTEP:";
      _endpoint = &endpointImpl->second;
      _endpoint->handleRequest(detachedSession);
      return;
    }

    _logger << "not found: " << path;
    // Schedule not found response
    detachedSession->write(notFoundResponse(_request));
  }

#pragma mark - State

  req_type request() {
    return _request;
  }

#pragma mark - Writing messages

  // Schedule a write
  void write(resp_type res) {
    std::cout << "write" << std::endl;
    std::cout.flush();

    auto self = shared_from_this();

    http::write(_socket, std::move(res));

    //http::async_write(
            //self->_socket,
            //res,
            //beast::bind_front_handler(
                //&Session::on_write,
                //self->shared_from_this(),
                //res.need_eof()));
  }

  void fail(beast::error_code ec, const std::string &what) {
    auto message = what + " and: " + ec.message();
    _logger << message;
  }

  // Schedule an error message
  void error(const std::string &message) {
    std::cout << "error" << std::endl;
    std::cout.flush();
    auto res = errorResponse(_request, message);
    http::write(_socket, std::move(res));
  }
};

#pragma mark - Server


void SemanticHTTPServer::run() {
    net::dispatch(
        _acceptor.get_executor(),
        beast::bind_front_handler(
            &SemanticHTTPServer::doAccept,
            this->shared_from_this()));
}

void SemanticHTTPServer::doAccept() {
    // The new connection gets its own strand
    _acceptor.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(
            &SemanticHTTPServer::onAccept,
            shared_from_this()));
}

void SemanticHTTPServer::onAccept(beast::error_code ec, socket_type socket) {
  if (ec) {
    std::cerr << ec.message() << "accept";
    return;
  } else {
    // Start a new Session.
    std::make_shared<Session>(std::move(socket), _context)->start();
  }

  std::cout << "accept again" << std::endl;
  doAccept();
}

#pragma mark - Endpoint impl

EndpointImpl makeStatusEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    resp_type res;
    res.result(http::status::ok);
    res.version(session->request().version());
    res.set(HeaderKeyServer, HeaderValueServer);
    res.set(HeaderKeyContentType, HeaderValueContentTypeJSON);
    res.body() = std::string("{}");
    res.set(http::field::content_length, boost::lexical_cast<std::string>(res.body().size()));
    session->write(res);
  });
}

EndpointImpl makeShutdownEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    session->logger() << "Recieved Shutdown Request";
    resp_type res;
    res.result(http::status::ok);
    res.version(session->request().version());
    res.set(HeaderKeyServer, HeaderValueServer);
    res.set(HeaderKeyContentType, HeaderValueContentTypeJSON);
    session->logger() << "Shutting down...";
    session->write(res);
    exit(0);
  });
}

EndpointImpl::EndpointImpl(EndpointFn start) : _start(start) {
}

void EndpointImpl::handleRequest(std::shared_ptr<Session> session) {
  auto logger = session->logger();
  logger << "HANDLE_REQUEST";
  logger << session->request().target();
  this->_start(session);
}

using boost::property_tree::ptree;
using boost::property_tree::read_json;

ptree readJSONPostBody(std::string body) {
  ptree pt;
  std::istringstream is(body);
  read_json(is, pt);
  return pt;
}

template <typename T>
const std::vector<T> as_vector(ptree const &pt, ptree::key_type const &key) {
  std::vector<T> r;
  for (auto &item : pt.get_child(key))
    r.push_back(item.second.get_value<T>());
  return r;
}

// Make completions endpoint returns an endpoint that
// handles basic completion requests
//
// @param flags: an array of string flags
// @param contents: the current files
// @param line: the users line
// @param column: the users column
// @param file_name: the name of the users file
EndpointImpl makeCompletionsEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    // Parse in data
    auto logger = session->logger();
    auto bodyString = session->request().body();
    logger << bodyString;
    auto bodyJSON = readJSONPostBody(bodyString);

    auto fileName = bodyJSON.get<std::string>("file_name");
    auto column = bodyJSON.get<int>("column") - 1;
    auto line = bodyJSON.get<int>("line");
    auto contents = bodyJSON.get<std::string>("contents");
    auto flags = as_vector<std::string>(bodyJSON, "flags");
    auto query = bodyJSON.get<std::string>("query");
    logger << "file_name:" << fileName;
    logger << "column:" << column;
    logger << "line:" << line;
    logger << "query:" << query;
    for (auto &f : flags) {
      logger << "flags:" << f;
    }

    using namespace ssvim;
    SwiftCompleter completer(session->logger().level());

    auto files = std::vector<UnsavedFile>();
    auto unsaved = UnsavedFile();
    unsaved.contents = contents;
    unsaved.fileName = fileName;
    files.push_back(unsaved);

    logger << "SEND_REQ";
    auto candidates = completer.CandidatesForLocationInFile(
        fileName, line, column, files, flags, query);

    logger << "GOT_CANDIDATES";
    session->logger().log(LogLevelExtreme, candidates);
    // Build out response
    resp_type res;
    res.result(http::status::ok);
    res.version(session->request().version());
    res.insert(HeaderKeyServer, HeaderValueServer);
    res.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
    res.body() = candidates;
    session->write(res);
  });
}

// Make completions endpoint returns an endpoint that
// handles basic completion requests
//
// @param flags: an array of string flags
// @param contents: the current files
// @param file_name: the name of the users file
EndpointImpl makeDiagnosticsEndpoint() {
  return EndpointImpl([&](std::shared_ptr<Session> session) {
    // Parse in data
    auto bodyString = session->request().body();
    session->logger() << bodyString;
    auto bodyJSON = readJSONPostBody(bodyString);

    auto fileName = bodyJSON.get<std::string>("file_name");
    auto contents = bodyJSON.get<std::string>("contents");
    auto flags = as_vector<std::string>(bodyJSON, "flags");
    session->logger() << "file_name:" << fileName;
    //for (auto &f : flags) {
      //session->logger().log(LogLevelInfo, "flags:", f);
    //}

    using namespace ssvim;
    SwiftCompleter completer(session->logger().level());

    auto files = std::vector<UnsavedFile>();
    auto unsaved = UnsavedFile();
    unsaved.contents = contents;
    unsaved.fileName = fileName;
    files.push_back(unsaved);

    session->logger() << "SEND_REQ";
    auto diagnostics = completer.DiagnosticsForFile(fileName, files, flags);

    session->logger() << "GOT_DIAGNOSTICS";
    session->logger().log(LogLevelExtreme, diagnostics);
    // Build out response
    resp_type res;
    res.result(http::status::ok);
    res.version(session->request().version());
    res.insert(HeaderKeyServer, HeaderValueServer);
    res.insert(HeaderKeyContentType, HeaderValueContentTypeJSON);
    res.body() = diagnostics;
    session->write(res);
  });
}

EndpointImpl makeSlowTestEndpoint() {
  return EndpointImpl([](std::shared_ptr<Session> session) {
    // Wait for 10 seconds to write hello world.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     session->logger() << "Enter main: ";
                     session->logger() << session->request().target();

                     resp_type res;
                     res.result(http::status::ok);
                     res.version(session->request().version());
                     res.set(HeaderKeyServer, HeaderValueServer);
                     res.set(HeaderKeyContentType, HeaderValueContentTypeJSON);
                     res.body() = "Hello World";
                     session->write(res);
                   });
  });
}

resp_type errorResponse(req_type request, std::string message) {
  resp_type res;
  res.result(500);
  res.reason("Internal Error");
  res.version(request.version());
  res.set(HeaderKeyServer, HeaderValueServer);
  res.set(HeaderKeyContentType, HeaderValueContentTypeJSON);
  res.body() = std::string{"An internal error occurred"} + message;
  return res;
}

resp_type notFoundResponse(req_type request) {
  resp_type res;
  res.result(404);
  res.reason("Not Found");
  res.version(request.version());
  res.set(HeaderKeyServer, HeaderValueServer);
  res.set(HeaderKeyContentType, HeaderValueContentTypeJSON);
  res.body() = std::string("Endpoint: '") + std::string(request.target()) + std::string("' not found");
  return res;
}

} // namespace http
} // namespace ssvim
