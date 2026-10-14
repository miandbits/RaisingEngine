#include "raising/runtime.hpp"
#include <iostream>
#include <string>
int main() {
  std::ios::sync_with_stdio(false);
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.size() > 64 * 1024 * 1024) {
      std::cout << "{\"ok\":false,\"error\":{\"code\":\"REQUEST_TOO_LARGE\",\"message\":\"Maximum "
                   "request size is 64 MiB.\",\"path\":\"/\"}}\n"
                << std::flush;
      continue;
    }
    std::cout << raising::process(line) << '\n' << std::flush;
  }
}
