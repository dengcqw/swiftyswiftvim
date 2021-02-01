#import "Logging.hpp"
#import "SwiftCompleter.hpp"
#import <dispatch/dispatch.h>
#import <iostream>
#import <string>
#import <functional>

using namespace ssvim;
using namespace std;

struct Runner {
  std::string complete(std::string fileName, std::string fileContents,
                       std::vector<std::string> flags, unsigned line,
                       unsigned column) {
    auto completer = SwiftCompleter(LogLevelExtreme);
    auto files = std::vector<UnsavedFile>();
    auto unsavedFile = UnsavedFile();
    unsavedFile.contents = fileContents;
    unsavedFile.fileName = fileName;

    files.push_back(unsavedFile);

    std::cout << "in complete" << std::endl;
    auto result = completer.CandidatesForLocationInFile(fileName, line, column,
                                                        files, flags, std::string());

    return result;
  }
};

std::string contents = "import UIKit; UIView.";
using namespace ssvim;
static ssvim::Logger logger(LogLevelInfo);

int wrapped_main() {
  Runner runner;

  auto exampleFilePath = "CBCB056E-0FC3-403B-9D91-556A88BD3F61.swift";
  vector<string> flags;
  flags.push_back("-c");
  flags.push_back(exampleFilePath);
  flags.push_back("-sdk");
  flags.push_back("/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk");
  flags.push_back("-target");
  flags.push_back("x86_64-apple-ios12.1-simulator");

  auto result = runner.complete(exampleFilePath, contents, flags, 1, 21);
  std::cout << "complete done" << std::endl;
  logger << result;
  logger << "Done";
  exit(0);
}

int main() {
  logger << "Running Test Driver";
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
    wrapped_main();
  });
  dispatch_main();
}
