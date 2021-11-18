#include "LZ77.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

int main(int argc, char* argv[])
{
  srand(time(0));

  LZ77 test(12, 512);
  
  std::string str = "";
  if (1 < argc)
  {
    std::ifstream instrm(argv[1], std::ios::binary);
    if (true == instrm.good())
    {
      std::stringstream stream;
      stream << instrm.rdbuf();
      str = stream.str();
      instrm.close();
    }
  }
  else
  {
    for (size_t i = 0; i < 1024; ++i)
    {
      str += 0x41 + (rand() % 2);
    }
  }

  float ratio = 0;
  std::string comp_str;
  std::string decomp_str;

  std::clock_t bTime = std::clock();
  test.Compress(str, comp_str, ratio);

  std::clock_t cTime = std::clock();
  test.Decompress(comp_str, decomp_str);

  std::clock_t dTime = std::clock();

  std::cout << "Compression Time: " << (((cTime - bTime) * 1000) / CLOCKS_PER_SEC) << std::endl;
  std::cout << "Deompression Time: " << (((dTime - cTime) * 1000) / CLOCKS_PER_SEC) << std::endl;
  std::cout << "Compression Size: " << comp_str.size() << std::endl;
  std::cout << "Compression Ratio: " << std::to_string(ratio).c_str() << std::endl;
  std::cout << "Size Check: " << (str.length() == decomp_str.length() ? "PASS" : "FAIL") << std::endl;
  std::cout << "Integrity Check: " << (0 == memcmp(str.data(), decomp_str.data(), str.length()) ? "PASS" : "FAIL") << std::endl;

  return 0;
}