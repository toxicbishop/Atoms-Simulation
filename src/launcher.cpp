#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Helper to launch a process
void launch(const std::string& command) {
    std::cout << "Launching: " << command << "...\n";
    // Using system() for simplicity and cross-platform compatibility for this use case
    // The command string will be constructed to run the executable
    std::string full_command = command;
#ifdef _WIN32
    // On Windows, system() works fine for console apps.
    // To run without blocking the launcher, we could use start, but blocking is fine here.
#else
    full_command = "./" + command;
#endif
    int ret = std::system(full_command.c_str());
    if (ret != 0) {
        std::cerr << "Error: Failed to launch " << command << " (Return code: " << ret << ")\n";
        std::cerr << "Make sure the executable exists in the current directory or bin/ folder.\n";
    }
}

int main() {
    std::vector<std::pair<std::string, std::string>> options = {
        {"2D Bohr Model (Standard)", "bin\\atom.exe"},
        {"Realtime 3D Model (Interactive)", "bin\\atom_realtime.exe"},
        {"Atom Excitation Model (Electron Jumping)", "bin\\atom_excitation.exe"},
        {"Wave Atom 2D", "bin\\wave_atom_2d.exe"},
        {"Raytracer (High Fidelity - WARNING: Needs Dedicated GPU/CUDA)", "bin\\atom_raytracer.exe"},
        // Add more options here as needed
    };

    while (true) {
        // Clear screen essentially
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
            if (selection >= 1 && selection <= (int)options.size()) {
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
