#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

bool is_digits(const std::string &str) {
  if (str.empty()) {
    return false;
  }
  return std::all_of(str.begin(), str.end(), ::isdigit);  // C++11
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Provide the file as arguments" << std::endl;
    return 1;
  }

  auto filename = argv[1];

  std::string line;
  std::ifstream dump(filename);
  if (!dump.is_open()) {
    std::cerr << "Could not open file `" << filename << "`" << std::endl;
    return 1;
  }

  std::vector<long long> numbers;
  while (getline(dump, line)) {
    if (!is_digits(line)) {
      std::cout << "Line contains more than just a positive integer, skipping"
                << std::endl;
      continue;
    }
    std::string::size_type sz = 0;
    long long ll = std::stoll(line, &sz, 10);
    numbers.push_back(ll);
  }

  std::vector<long long> filtered;

  std::cout
      << "================================================================"
      << std::endl;
  std::cout << "Keep only values lower than 40000ns" << std::endl;
  std::copy_if(numbers.begin(), numbers.end(), std::back_inserter(filtered),
               [](int i) { return i < 40000; });

  sort(filtered.begin(), filtered.end());

  long long sum = std::accumulate(filtered.begin(), filtered.end(), 0ll);
  double mean = double(sum) / filtered.size();

  auto max_idx = filtered.size() - 1;
  auto [min_elem, max_elem] =
      std::minmax_element(filtered.begin(), filtered.end());

  std::cout << "Samples #: " << filtered.size() << std::endl;
  std::cout << "Skipped: " << numbers.size() - filtered.size() << std::endl;
  std::cout << "(Max, Min): " << *min_elem << ", " << *max_elem << std::endl;
  std::cout << "Average: " << mean << "ns" << std::endl;
  std::cout << "25th %-tile: " << filtered[std::roundl(max_idx * 0.25)] << "ns"
            << std::endl;
  std::cout << "50th %-tile: " << filtered[std::roundl(max_idx * 0.50)] << "ns"
            << std::endl;
  std::cout << "75th %-tile: " << filtered[std::roundl(max_idx * 0.75)] << "ns"
            << std::endl;
  std::cout << "90th %-tile: " << filtered[std::roundl(max_idx * 0.90)] << "ns"
            << std::endl;
  std::cout << "95th %-tile: " << filtered[std::roundl(max_idx * 0.95)] << "ns"
            << std::endl;
  std::cout << "98th %-tile: " << filtered[std::roundl(max_idx * 0.98)] << "ns"
            << std::endl;
  std::cout << "99th %-tile: " << filtered[std::roundl(max_idx * 0.99)] << "ns"
            << std::endl;

  dump.close();

  return 0;
}
