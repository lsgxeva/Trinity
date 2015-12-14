#include "gtest/gtest.h"

#include "mocca/log/LogManager.h"
#include "mocca/log/ConsoleLog.h"


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  using mocca::LogManager;
#ifdef MUI_TEST_LOGGING
  LogManager::initialize(LogManager::LogLevel::Debug, true);
#else
  LogManager::initialize(LogManager::LogLevel::None, true);
#endif
  auto log = new mocca::ConsoleLog();
  LogMgr.addLog(log);

  return RUN_ALL_TESTS();
}