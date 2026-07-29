#include <cstring>
#include <vector>

inline int compare_bytes(const std::vector<std::byte> &a,
                         const std::vector<std::byte> &b) {
  int res{std::memcmp(a.data(), b.data(), std::min(a.size(), b.size()))};
  if (res == 0) {
    if (a.size() < b.size()) {
      res = -1;
    } else if (a.size() > b.size()) {
      res = 1;
    } else {
      res = 0;
    }
  }
  return res;
};
