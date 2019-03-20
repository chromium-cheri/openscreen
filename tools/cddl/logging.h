// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_LOGGING_H_
#define TOOLS_CDDL_LOGGING_H_

#include <string>

class Logger {
 private:
  // Limit calling the constructor/destructor to from within this same class.
  Logger();
  ~Logger();

  // File path bring written to.
  std::string file_path;

  // File handle for the logging.
  int file_fd;

  // Represents whether this instance has been initialized.
  bool is_initialized;

  // Exits the program if initialization has not occured.
  void VerifyInitialized();

  // fprintf doesn't like passing strings as parameters, so use overloads to
  // convert all C++ std::string types into C strings.
  template<class T>
  T MakePrintable(T data) {
    return data;
  }

  const char* MakePrintable(std::string data);

  // Writes a log message to this instance of Logger's text file.
  template<typename... Args>
  void WriteLog(const std::string& message, Args&&... args) {
    VerifyInitialized();
    // NOTE: wihout the #pragma suppressions, the below line fails. There is a
    // warning generated since the compiler is attempting to prevent a string
    // format vulnerability. This is not a risk for us since this code is only
    // used at compile time. The below #pragma commands suppress the warning for
    // just the one dprintf(...) line.
    // For more details: https://www.owasp.org/index.php/Format_string_attack
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wformat-security"
    dprintf(this->file_fd, message.c_str(),
            MakePrintable(std::forward<Args>(args))...);
    #pragma clang diagnostic pop
  }

  // Writes an error message to this instance of Logger's text file.
  template<typename... Args>
  void WriteError(const std::string& message, Args&&... args) {
    this->WriteLog("ERROR: " + message, std::forward<Args>(args)...);
  }

  // Singleton instance of logger. At the beginning of runtime it's initiated to
  // nullptr due to zero initialization.
  static Logger* singleton;

 public:
  // Creates and initializes the logging file associated with this logger.
  void Initialize(std::string output_dir);

  // Gets the file path associated with this logger.
  static std::string GetFilePath();

  // Writes a log to the global singleton instance of Logger.
  template<typename... Args>
  static void Log(const std::string& message, Args&&... args) {
    Logger::Get()->WriteLog(std::forward<const std::string>(message),
                            std::forward<Args>(args)...);
  }

  // Writes an error to the global singleton instance of Logger.
  template<typename... Args>
  static void Error(const std::string& message, Args&&... args) {
    Logger::Get()->WriteError(std::forward<const std::string>(message),
                              std::forward<Args>(args)...);
  }

  // Returns the singleton instance of Logger.
  static Logger* Get();
};

#endif  // TOOLS_CDDL_LOGGING_H_
