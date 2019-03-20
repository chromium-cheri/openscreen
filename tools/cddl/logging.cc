// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/logging.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <cinttypes>
#include <iostream>
#include <string>
#include <utility>

const char* Logger::MakePrintable(std::string data) {
  return data.c_str();
}

void Logger::Initialize(std::string output_dir) {
  std::string temp_path = output_dir + "/CddlFileGeneration.XXXXXX.log";
  char* file_path_c_str = strdup(temp_path.c_str());
  this->file_fd = mkstemps(file_path_c_str, 4);
  this->file_path = std::string(file_path_c_str);
  if (this->file_fd == -1) {
    std::cerr << "Error initializing log file\n";
    exit(1);
  }

  this->is_initialized = true;

  this->WriteLog("CDDL GENERATION TOOL");
  this->WriteLog("---------------------------------------------\n");
}

void Logger::VerifyInitialized() {
  if (!this->is_initialized) {
    std::cerr << "Error: Attempting to log to uninitialized logger.\n";
    exit(1);
  }
}

Logger::~Logger() {
  close(this->file_fd);
}

Logger::Logger() {
  this->is_initialized = false;
}

std::string Logger::GetFilePath() {
  return Logger::Get()->file_path;
}

Logger* Logger::Get() {
  return Logger::singleton;
}

auto Logger::singleton = new Logger();
