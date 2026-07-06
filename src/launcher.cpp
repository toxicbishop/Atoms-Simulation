#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

/// Build the full path to an executable in the bin/ directory.
/// Appends .exe on Windows. Quotes the path to handle spaces.
static std::string build_command(const std::string& base_name) {
    std::string exe_name = base_name;
#ifdef _WIN32
    exe_name += ".exe";
#endif

    // Check possible locations: bin/Release/ (MSVC), bin/ (GCC/Clang), or current dir
    fs::path p1 = fs::current_path() / "bin" / "Release" / exe_name;
    fs::path p2 = fs::current_path() / "bin" / exe_name;
    
    fs::path target = p1;
    if (!fs::exists(p1) && fs::exists(p2)) {
        target = p2;
    }

    // Quote the path to handle directories with spaces
    return "\"" + target.generic_string() + "\"";
}

/// Launch a child process by base executable name.
static void launch(const std::string& base_name) {
    std::string command = build_command(base_name);
    std::cout << "Launching: " << command << "...\n";

    int ret = std::system(command.c_str());
    if (ret != 0) {
        std::cerr << "Error: Failed to launch " << base_name
                  << " (return code: " << ret << ")\n";
        std::cerr << "Make sure the executable exists in the bin/ folder.\n";
    }
}

int main() {
    // Each entry: { display label, base executable name (no path, no extension) }
    const std::vector<std::pair<std::string, std::string>> options = {
        {"2D Bohr Model (Standard)",                                      "atom"},
        {"Realtime 3D Model (Interactive)",                               "atom_realtime"},
        {"Atom Excitation Model (Electron Jumping)",                      "atom_excitation"},
        {"Wave Atom 2D",                                                  "wave_atom_2d"},
        {"Raytracer (High Fidelity - WARNING: Needs Dedicated GPU/CUDA)", "atom_raytracer"},
    };

    while (true) {
        std::cout << "\n========================================\n";
        std::cout << "   Atoms Simulation Launcher\n";
        std::cout << "========================================\n\n";

        for (size_t i = 0; i < options.size(); ++i) {
            std::cout << "  [" << (i + 1) << "] " << options[i].first << "\n";
        }
        std::cout << "  [Q] Quit\n";
        std::cout << "\nSelect a simulation to run: ";

        std::string input;
        std::cin >> input;

        if (input == "q" || input == "Q") {
            break;
        }

        try {
            int selection = std::stoi(input);
            if (selection >= 1 && selection <= static_cast<int>(options.size())) {
                launch(options[selection - 1].second);
            } else {
                std::cout << "Invalid selection. Please try again.\n";
            }
        } catch (...) {
            std::cout << "Invalid input. Please enter a number.\n";
        }
    }

    return 0;
}
