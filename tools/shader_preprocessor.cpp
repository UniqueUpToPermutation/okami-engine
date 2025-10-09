#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <vector>
#include <regex>

class ShaderPreprocessor {
private:
    std::unordered_set<std::string> m_includedFiles;
    std::filesystem::path m_baseDirectory;

    std::string processFile(const std::filesystem::path& filePath) {
        // Convert to absolute path for include tracking
        auto absolutePath = std::filesystem::absolute(filePath);
        std::string absolutePathStr = absolutePath.string();
        
        // Check for circular includes
        if (m_includedFiles.find(absolutePathStr) != m_includedFiles.end()) {
            throw std::runtime_error("Circular include detected: " + absolutePathStr);
        }
        
        m_includedFiles.insert(absolutePathStr);
        
        // Read the file
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filePath.string());
        }
        
        std::string result;
        std::string line;
        int lineNumber = 1;
        
        while (std::getline(file, line)) {
            // Check if this line is an include directive
            if (processIncludeLine(line, filePath.parent_path(), result)) {
                // Line was processed as an include, continue to next line
            } else {
                // Regular line, add it to the result
                result += line + "\n";
            }
            lineNumber++;
        }
        
        // Remove this file from the included set (for potential reuse in other branches)
        m_includedFiles.erase(absolutePathStr);
        
        return result;
    }
    
    bool processIncludeLine(const std::string& line, const std::filesystem::path& currentDirectory, std::string& result) {
        // Regex to match #include "filename" or #include <filename> (supports .wgsl, .glsl, .vs, .fs, etc.)
        std::regex includeRegex(R"(\s*#include\s+[\"<]([^\"<>]+)[\">]\s*)");
        std::smatch matches;
        
        if (std::regex_match(line, matches, includeRegex)) {
            std::string includeFile = matches[1].str();
            
            // Resolve the include path relative to the current file's directory
            std::filesystem::path includePath;
            if (std::filesystem::path(includeFile).is_absolute()) {
                includePath = includeFile;
            } else {
                includePath = currentDirectory / includeFile;
            }
            
            // Try to find the file
            if (!std::filesystem::exists(includePath)) {
                // Try relative to base directory as fallback
                auto fallbackPath = m_baseDirectory / includeFile;
                if (std::filesystem::exists(fallbackPath)) {
                    includePath = fallbackPath;
                } else {
                    throw std::runtime_error("Include file not found: " + includeFile + 
                                           " (searched in " + currentDirectory.string() + 
                                           " and " + m_baseDirectory.string() + ")");
                }
            }
            
            // Add a comment indicating the include
            result += "// #include \"" + includeFile + "\" - Start\n";
            
            // Process the included file
            std::string includedContent = processFile(includePath);
            result += includedContent;
            
            // Add a comment indicating the end of the include
            result += "// #include \"" + includeFile + "\" - End\n";
            
            return true; // Line was processed as an include
        }
        
        return false; // Not an include line
    }

public:
    ShaderPreprocessor(const std::filesystem::path& baseDirectory) 
        : m_baseDirectory(baseDirectory) {}
    
    void preprocessFile(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        try {
            m_includedFiles.clear();
            
            std::string processedContent = processFile(inputFile);
            
            // Create output directory if it doesn't exist
            std::filesystem::create_directories(outputFile.parent_path());
            
            // Write the processed content to the output file
            std::ofstream output(outputFile);
            if (!output.is_open()) {
                throw std::runtime_error("Could not create output file: " + outputFile.string());
            }
            
            output << processedContent;
            output.close();
            
            std::cout << "Preprocessed " << inputFile << " -> " << outputFile << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << inputFile << ": " << e.what() << std::endl;
            throw;
        }
    }
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <input_shader> <output_shader> [base_directory]\n";
    std::cout << "  input_shader    - Input shader file to preprocess (.wgsl, .glsl, .vs, .fs, etc.)\n";
    std::cout << "  output_shader   - Output file path\n";
    std::cout << "  base_directory  - Base directory for resolving includes (optional, defaults to input file directory)\n";
    std::cout << "\nThe preprocessor resolves #include \"filename\" directives recursively.\n";
    std::cout << "Supports WGSL (.wgsl) and GLSL (.glsl, .vs, .fs, etc.) shader files.\n";
    std::cout << "Include files are searched relative to the current file's directory first,\n";
    std::cout << "then relative to the base directory.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        std::filesystem::path inputFile(argv[1]);
        std::filesystem::path outputFile(argv[2]);
        
        std::filesystem::path baseDirectory;
        if (argc >= 4) {
            baseDirectory = argv[3];
        } else {
            baseDirectory = inputFile.parent_path();
        }
        
        if (!std::filesystem::exists(inputFile)) {
            std::cerr << "Error: Input file does not exist: " << inputFile << std::endl;
            return 1;
        }
        
        ShaderPreprocessor preprocessor(baseDirectory);
        preprocessor.preprocessFile(inputFile, outputFile);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}