#include <iostream>
#include <SDL.h>
#include <filesystem>
#include <vector>
#include <string>
#include "VideoPlayer.h"

// ImGui includes
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

// Video file extensions
const std::vector<std::string> VIDEO_EXTENSIONS = {".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"};

bool hasVideoExtension(const std::string& filename) {
    for (const auto& ext : VIDEO_EXTENSIONS) {
        if (filename.size() >= ext.size() &&
            filename.substr(filename.size() - ext.size()) == ext) {
            return true;
        }
    }
    return false;
}

void showFileSelectionDialog(std::string& selectedPath, bool& videoLoaded, VideoPlayer& player, bool& running) {
    static std::filesystem::path currentPath = std::filesystem::current_path();
    static std::vector<std::filesystem::path> directories;
    static std::vector<std::filesystem::path> videoFiles;
    static std::vector<std::string> audioDevices;
    static int selectedAudioDeviceIndex = 0;

    // Refresh directory contents
    if (directories.empty() && videoFiles.empty()) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                if (entry.is_directory()) {
                    directories.push_back(entry.path());
                } else if (entry.is_regular_file() && hasVideoExtension(entry.path().filename().string())) {
                    videoFiles.push_back(entry.path());
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error reading directory: " << e.what() << std::endl;
        }
    }

    // Get available audio devices
    if (audioDevices.empty()) {
        audioDevices = AudioManager::getAvailableAudioDevices();
        audioDevices.insert(audioDevices.begin(), "Default Audio Device");
        
        // Set initial selection based on player's current device
        std::string currentDevice = player.getAudioDevice();
        if (currentDevice.empty()) {
            selectedAudioDeviceIndex = 0; // Default
        } else {
            for (size_t i = 1; i < audioDevices.size(); ++i) {
                if (audioDevices[i] == currentDevice) {
                    selectedAudioDeviceIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Select Video File", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Current Directory: %s", currentPath.string().c_str());

    // Audio device selection
    ImGui::Separator();
    ImGui::Text("Audio Output Device:");
    if (ImGui::Combo("##AudioDevice", &selectedAudioDeviceIndex, 
                     [](void* data, int idx, const char** out_text) {
                         std::vector<std::string>* devices = static_cast<std::vector<std::string>*>(data);
                         if (idx < 0 || idx >= static_cast<int>(devices->size())) return false;
                         *out_text = (*devices)[idx].c_str();
                         return true;
                     }, &audioDevices, static_cast<int>(audioDevices.size()))) {
        // Update player's selected audio device
        std::string deviceName = (selectedAudioDeviceIndex == 0) ? "" : audioDevices[selectedAudioDeviceIndex];
        player.setAudioDevice(deviceName);
    }

    // Parent directory button
    if (currentPath.has_parent_path() && ImGui::Button(".. (Parent Directory)")) {
        currentPath = currentPath.parent_path();
        directories.clear();
        videoFiles.clear();
    }

    ImGui::Separator();
    ImGui::Text("Directories:");

    // Directory list
    for (const auto& dir : directories) {
        if (ImGui::Button(dir.filename().string().c_str())) {
            currentPath = dir;
            directories.clear();
            videoFiles.clear();
            break;
        }
    }

    ImGui::Separator();
    ImGui::Text("Video Files:");

    // Video files list
    for (const auto& file : videoFiles) {
        if (ImGui::Button(file.filename().string().c_str())) {
            selectedPath = file.string();
            if (player.loadVideo(selectedPath)) {
                videoLoaded = true;
                player.play();
            } else {
                // Show error message
                ImGui::OpenPopup("Load Error");
            }
            break;
        }
    }

    // Error popup
    if (ImGui::BeginPopupModal("Load Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Failed to load video file:");
        ImGui::Text("%s", selectedPath.c_str());
        ImGui::Separator();
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
            selectedPath.clear();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::Text("Or enter file path manually:");
    static char filePathBuffer[512] = "";
    ImGui::InputText("File Path", filePathBuffer, sizeof(filePathBuffer));

    if (ImGui::Button("Load from Path")) {
        selectedPath = filePathBuffer;
        if (player.loadVideo(selectedPath)) {
            videoLoaded = true;
            player.play();
        } else {
            ImGui::OpenPopup("Load Error");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Quit")) {
        running = false;
    }

    ImGui::End();
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create window and renderer for ImGui
    SDL_Window* window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Application state
    std::string selectedVideoPath;
    bool videoLoaded = false;
    VideoPlayer player(renderer);

    // Check if video file was provided as command line argument
    if (argc > 1) {
        selectedVideoPath = argv[1];
        if (player.loadVideo(selectedVideoPath)) {
            videoLoaded = true;
            player.play();
        } else {
            std::cerr << "Failed to load video: " << selectedVideoPath << std::endl;
            selectedVideoPath.clear();
        }
    }

    // Main event loop
    SDL_Event event;
    bool running = true;
    bool show_controls = true;

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (!videoLoaded) {
                        // File selection mode
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            running = false;
                        }
                    } else {
                        // Video playback mode
                        switch (event.key.keysym.sym) {
                            case SDLK_SPACE:
                                if (player.isPlaying()) {
                                    player.pause();
                                } else {
                                    player.play();
                                }
                                break;
                            case SDLK_s:
                                player.stop();
                                break;
                            case SDLK_f:
                                player.toggleFullscreen();
                                break;
                            case SDLK_UP:
                                player.volumeUp();
                                break;
                            case SDLK_DOWN:
                                player.volumeDown();
                                break;
                            case SDLK_LEFT:
                                player.seek(-10.0f);
                                break;
                            case SDLK_RIGHT:
                                player.seek(10.0f);
                                break;
                            case SDLK_ESCAPE:
                                videoLoaded = false;
                                selectedVideoPath.clear();
                                player.stop();
                                break;
                            case SDLK_TAB:
                                show_controls = !show_controls;
                                break;
                        }
                    }
                    break;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (!videoLoaded) {
            // File selection screen
            showFileSelectionDialog(selectedVideoPath, videoLoaded, player, running);
        } else {
            // Video playback GUI
            if (show_controls) {
                ImGui::Begin("Video Controls", &show_controls, ImGuiWindowFlags_AlwaysAutoResize);

                if (ImGui::Button("Stop & Select New Video")) {
                    videoLoaded = false;
                    selectedVideoPath.clear();
                    player.stop();
                    ImGui::End();
                    continue;
                }

                // Play/Pause/Stop buttons
                if (ImGui::Button(player.isPlaying() ? "Pause" : "Play")) {
                    if (player.isPlaying()) {
                        player.pause();
                    } else {
                        player.play();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    player.stop();
                }
                ImGui::SameLine();
                if (ImGui::Button("Fullscreen")) {
                    player.toggleFullscreen();
                }

                // Volume control
                static float volume = 1.0f;
                if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                    player.setVolume(volume);
                }

                // Seek control (placeholder)
                static float seek_pos = 0.0f;
                if (ImGui::SliderFloat("Position", &seek_pos, 0.0f, 100.0f, "%.1f%%")) {
                    // TODO: Implement seeking
                }

                ImGui::Text("Press TAB to hide/show controls");
                ImGui::Text("ESC to return to file selection");

                ImGui::End();
            }

            // Update player
            player.update();
        }

        // Rendering
        if (videoLoaded) {
            // Video rendering is handled by VideoPlayer (clears and renders to main renderer)
            // ImGui will be rendered on top
        } else {
            // Clear screen for GUI
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderClear(renderer);
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    player.stop();

    // ImGui cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_Quit();

    return 0;
}