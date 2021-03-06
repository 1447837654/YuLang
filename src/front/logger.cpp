#include "front/logger.h"

#include <iostream>

#include "xstl/style.h"

using namespace yulang::front;

// definition of static member variables in logger
std::size_t Logger::error_num_, Logger::warning_num_;
bool Logger::warn_as_err_;

// TODO: show content of file

void Logger::LogFileInfo() const {
  using namespace xstl;
  std::cerr << style("B") << cur_file_ << ":";
  std::cerr << style("B") << line_pos_ << ":" << col_pos_ << ": ";
}

void Logger::LogRawError(std::string_view message) {
  using namespace xstl;
  // print error message
  std::cerr << style("Br") << "error: ";
  std::cerr << message << std::endl;
  // increase error number
  ++error_num_;
}

void Logger::LogError(std::string_view message) const {
  LogFileInfo();
  LogRawError(message);
}

void Logger::LogError(std::string_view message,
                      std::string_view id) const {
  using namespace xstl;
  LogFileInfo();
  // print error message
  std::cerr << style("Br") << "error: ";
  std::cerr << "id: " << id << ", " << message << std::endl;
  // increase error number
  ++error_num_;
}

// print warning message to stderr
void Logger::LogWarning(std::string_view message) const {
  using namespace xstl;
  // log all warnings as errors
  if (warn_as_err_) {
    LogError(message);
    return;
  }
  // print warning message
  LogFileInfo();
  std::cerr << style("Bp") << "warning: ";
  std::cerr << message << std::endl;
  // increase warning number
  ++warning_num_;
}
