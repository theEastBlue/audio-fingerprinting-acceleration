#include "fingerprint.h"
#include <iostream>
#include <vector>

int main() {
    std::vector<float> data = load_audio("test.mp3");

    std::cout << "loaded samples: " << data.size() << std::endl;

    std::string result = fingerprint(data.data(), (int)data.size());

    std::cout << result << std::endl;

    return 0;
}
