#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "painlessmesh/configuration.hpp"

#include "painlessmesh/base64.hpp"

#include "catch_utils.hpp"

SCENARIO("Base64 encoding can succesfully be decoded") {
  using namespace painlessmesh;
  auto bindata = randomString(100);
  auto enc = base64::encode(bindata);
  auto dec = base64::decode(enc);
  REQUIRE(dec.length() > 0);
  REQUIRE(dec == bindata);
}
