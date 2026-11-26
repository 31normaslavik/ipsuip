// SPDX-License-Identifier: MIT
#include "suip_model.h"
#include "test_util.h"

#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string read_file(const char *path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "usage: test_suip_model <continents.html> <ranges.html> "
                 "<ranges.txt>\n";
    return 2;
  }

  const std::string continents = read_file(argv[1]);
  const auto targets = parse_targets(continents, "https://suip.biz/");

  CHECK_EQ(targets.size(), static_cast<std::size_t>(2));
  CHECK(targets[0].level == TargetLevel::Continent);
  CHECK_EQ(targets[0].code, std::string("EU"));
  CHECK_EQ(targets[0].location, std::string("Europe"));
  CHECK_EQ(targets[0].address,
           std::string("https://suip.biz/?act=all-country-ip&continent=EU"));
  CHECK_EQ(targets[0].all_ip_direct_download,
           std::string("https://suip.biz/?act=all-country-ip&continent=EU&all"));
  CHECK_EQ(targets[1].all_ip_direct_download,
           std::string("https://suip.biz/?act=all-country-ip&continent=AS&all-download"));
  CHECK_EQ(level_name(TargetLevel::Province), std::string("province"));

  const auto ranges = parse_ip_ranges(read_file(argv[2]));
  CHECK_EQ(ranges.size(), static_cast<std::size_t>(2));
  CHECK_EQ(ranges[0], std::string("192.168.1.0-192.168.1.255"));
  CHECK_EQ(ranges[1], std::string("2001:db8::-2001:db8::ffff"));

  const auto plain = extract_ip_ranges(read_file(argv[3]));
  CHECK_EQ(plain.size(), static_cast<std::size_t>(2));

  TEST_REPORT("suip_model");
}
