// PNG / JPEG to KTX2 converter with mipmap generation.
// This is a thin command-line wrapper around TextureProcessor.
// Usage: png2ktx input.[png|jpg|jpeg] output.ktx2 [--quiet] [--linear]

#include "texture_processor.hpp"
#include <iostream>
#include <cstring>
#include <filesystem>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " input.[png|jpg|jpeg] output.ktx2 [--quiet] [--linear]\n";
        return 1;
    }

    bool quiet  = false;
    bool linear = false;

    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (std::strcmp(argv[i], "--linear") == 0) {
            linear = true;
        } else {
            std::cerr << "Unknown flag: " << argv[i] << "\n";
            std::cerr << "Usage: " << argv[0]
                      << " input.[png|jpg|jpeg] output.ktx2 [--quiet] [--linear]\n";
            return 1;
        }
    }

    try {
        std::filesystem::path input(argv[1]);
        std::filesystem::path output(argv[2]);
        std::filesystem::create_directories(output.parent_path());

        TextureProcessorParams params;
        params.linearMips = linear;
        TextureProcessor processor(quiet, params);
        processor.Process(input, output);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
