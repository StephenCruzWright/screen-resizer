#include <iostream>

int main() {
  if (1 + 1 != 2) {
    std::cerr << "basic math test failed" << std::endl;
    return 1;
  }

  std::cout << "basic math test passed" << std::endl;
  return 0;
}
