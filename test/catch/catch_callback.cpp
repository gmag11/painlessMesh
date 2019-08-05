#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include <Arduino.h>

#include "catch_utils.hpp"

#include "painlessmesh/callback.hpp"

using namespace painlessmesh;

logger::LogClass Log;

SCENARIO("CallbackMap should hold multiple callbacks by ID") {
  GIVEN("A callback map with added callbacks") {
    auto cbl = callback::PackageCallbackList<int>();

    auto i = 0;
    auto j = 0;

    cbl.onPackage(1, [&i](int z) { ++i; });
    cbl.onPackage(1, [&j](int z) { ++j; });

    WHEN("We call execute") {
      auto cnt = cbl.execute(1, 0);
      REQUIRE(cnt == 2);
      THEN("The callbacks are called") {
        REQUIRE(i == 1);
        REQUIRE(j == 1);
      }
    }

    WHEN("We call execute on another event") {
      auto cnt = cbl.execute(2, 0);
      REQUIRE(cnt == 0);
      THEN("The callbacks are not called") {
        REQUIRE(i == 0);
        REQUIRE(j == 0);
      }
    }
  }
}
