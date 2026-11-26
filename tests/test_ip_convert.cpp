// SPDX-License-Identifier: MIT
#include "ip_convert.h"
#include "test_util.h"

#include <string>
#include <vector>

int main() {
  using ipsuip::range_to_cidr;
  using ipsuip::ranges_to_cidr;
  using ipsuip::sanitize_filename;

  CHECK_EQ(range_to_cidr("192.168.1.0-192.168.1.255"),
           (std::vector<std::string>{"192.168.1.0/24"}));
  CHECK_EQ(range_to_cidr("10.0.0.0-10.0.0.3"),
           (std::vector<std::string>{"10.0.0.0/30"}));
  CHECK_EQ(range_to_cidr("0.0.0.0-255.255.255.255"),
           (std::vector<std::string>{"0.0.0.0/0"}));
  CHECK_EQ(range_to_cidr("192.168.1.5-192.168.1.5"),
           (std::vector<std::string>{"192.168.1.5/32"}));
  CHECK_EQ(range_to_cidr("10.0.0.1-10.0.0.2"),
           (std::vector<std::string>{"10.0.0.1/32", "10.0.0.2/32"}));

  CHECK(range_to_cidr("not a range").empty());
  CHECK(range_to_cidr("300.1.1.1-300.1.1.2").empty());
  CHECK(range_to_cidr("10.0.0.2-10.0.0.1").empty());
  CHECK(range_to_cidr("10.0.0.0-10.0.0.0-10.0.0.1").empty());

  // Неподдержанные строки (включая IPv6) остаются как есть.
  CHECK_EQ(ranges_to_cidr({"2a03:d000::-2a03:d000::ffff"}),
           (std::vector<std::string>{"2a03:d000::-2a03:d000::ffff"}));
  CHECK_EQ(ranges_to_cidr({"192.168.0.0-192.168.0.255", "garbage"}),
           (std::vector<std::string>{"192.168.0.0/24", "garbage"}));

  CHECK_EQ(sanitize_filename("  ../Brest city: 1 "), "Brest_city_1");
  CHECK_EQ(sanitize_filename("..."), "unnamed");
  CHECK_EQ(sanitize_filename("Европа/Азия"), "Европа_Азия");

  TEST_REPORT("ip_convert");
}
