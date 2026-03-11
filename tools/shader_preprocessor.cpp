// Shader preprocessor — resolves #include directives in GLSL / WGSL files.
// This is a thin command-line wrapper around ShaderAssetProcessor.
// Usage: shader_preprocessor <input_shader> <output_shader> [base_directory]

#include "shader_processor.hpp"
#include <iostream>
#include <filesystem>
#include <stdexcept>

static void printUsage(const char* name) {
    std::cout << "Usage: " << name
              << " <input_shader> <output_shader> [base_directory]\n"
              << "  input_shader    - Shader file to preprocess\n"
              << "  output_shader   - Output file path\n"
              << "  base_directory  - Fallback directory for resolving includes\n"
              << "                    (defaults to the input file's directory)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        printUsage(argv[0]);
        return 1;
    }
    try {
        std::filesystem::path input(argv[1]);
        std::filesystem::path output(argv[2]);
        std::filesystem::path baseDir = (argc >= 4)
                                          ? std::filesystem::path(argv[3])
                                          : input.parent_path();

        if (!std::filesystem::exists(input)) {
            std::cerr << "Error: input file does not exist: " << input << "\n";
            return 1;
        }

        ShaderAssetProcessor processor(baseDir);
        processor.Process(input, output);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}