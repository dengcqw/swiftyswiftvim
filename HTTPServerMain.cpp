#import "Logging.hpp"
#include <memory>
#import "SemanticHTTPServer.hpp"

#import <boost/algorithm/string.hpp>
#import <boost/program_options.hpp>
#import <dispatch/dispatch.h>
#import <iostream>

static auto LogLevelWithProgramOptionLog(std::string option) {
  using namespace ssvim;
  if (option == "DEBUG") {
    return LogLevelExtreme;
  } else if (option == "INFO") {
    return LogLevelInfo;
  } else {
    // WARNING or other unknown types get minimal logging.
    return LogLevelError;
  }
}

int main(int ac, char const *av[]) {
  namespace po = boost::program_options;
  po::options_description desc("Options");

  desc.add_options()("root,r", po::value<std::string>()->default_value("."),
                     "Set the root directory for serving files")(
      "port,p", po::value<std::uint16_t>()->default_value(8081),
      "Set the port number for the server")(
      "ip", po::value<std::string>()->default_value("127.0.0.1"),
      "Set the IP address to bind to, \"0.0.0.0\" for all")(
      "threads,n", po::value<std::size_t>()->default_value(4),
      "Set the number of threads to use")
      // DEBUG, INFO, WARNING
      ("log,r", po::value<std::string>()->default_value("INFO"),
       "Set the logging level")("hmac-file-secret,r",
                                po::value<std::string>()->default_value("none"),
                                "Set the hmac secret");
  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);

  std::string root = vm["root"].as<std::string>();

  std::uint16_t port = vm["port"].as<std::uint16_t>();

  std::string ip = vm["ip"].as<std::string>();

  //std::size_t threads = vm["threads"].as<std::size_t>();
  std::string log = vm["log"].as<std::string>();

  using endpoint_type = boost::asio::ip::tcp::endpoint;
  using address_type = boost::asio::ip::address;
  using namespace ssvim;
  using namespace ssvim::http;

  std::cout << "__LISTENINGON: " << ip << ":" << port << std::endl;
  std::cout.flush();
  ServiceContext ctx("SomeSecret", LogLevelWithProgramOptionLog(
                                       boost::to_upper_copy<std::string>(log)));
  endpoint_type ep{address_type::from_string(ip), port};
  boost::asio::io_context ioc{1};
  std::make_shared<SemanticHTTPServer>(ioc, ep, root, ctx)->run();
  ioc.run();

  net::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&](beast::error_code const&, int) {
      // Stop the `io_context`. This will cause `run()`
      // to return immediately, eventually destroying the
      // `io_context` and all of the sockets in it.
      ioc.stop();
  });
  return 0;
}
