#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <Windows.h>
#include <unordered_set>

std::string ReplaceSubstring(std::string string, std::string substring, std::string replaceWith)
{
    return std::regex_replace(string, std::regex(substring), replaceWith);
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cout << "Usage: ReplaceGUIDwithCorrectOne <IncorrectGUIDsPath> <CorrectGUIDsPath> <UnityProjectPath>\n";
        return 1;
    }

    std::string IncorrectGUIDsPath = argv[1];
    std::string CorrectGUIDsPath = argv[2];
    std::string UnityProjectPath = argv[3];

    std::filesystem::path path1 = IncorrectGUIDsPath;
    std::filesystem::path path2 = CorrectGUIDsPath;
    std::filesystem::path path3 = UnityProjectPath;

    if (!std::filesystem::exists(path1) || !std::filesystem::exists(path2) || !std::filesystem::exists(path3)) {
        std::cout << "WTH? a path wasn't valid! Double check and try again.\n";
        return 1;
    }

    std::unordered_set<std::string> skipFolders = {
        "AnimationClip", "AnimatorController", "AnimatorOverrideController", "AudioClip", "AudioMixerController",
        "Avatar", "AvatarMask", "Cubemap", "Editor", "Material", "Mesh", "PhysicMaterial",
        "RenderTexture", "Resources", "Shader", "Sprite", "Texture2D", "VideoClip"
    };

    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(IncorrectGUIDsPath)) {
        if (entry.is_directory() && skipFolders.find(entry.path().filename().string()) != skipFolders.end()) {
            continue;
        }

        if (entry.path().extension() == ".meta") {
            std::ifstream ifs(entry.path());
            if (!ifs.is_open()) continue;

            // Get file contents
            std::string contentsOfWrongGUID((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            std::string incorrectGuid = contentsOfWrongGUID.substr(27, 32);

            for (const auto& entry2 : std::filesystem::recursive_directory_iterator(CorrectGUIDsPath)) {
                if (entry2.is_directory() && skipFolders.find(entry2.path().filename().string()) != skipFolders.end()) {
                    continue;
                }

                if (entry2.path().filename() == entry.path().filename()) {
                    std::ifstream ifs2(entry2.path());
                    if (!ifs2.is_open()) continue;

                    // Get file contents
                    std::string contentsOfCorrectGUID((std::istreambuf_iterator<char>(ifs2)), (std::istreambuf_iterator<char>()));
                    std::string CorrectGuid = contentsOfCorrectGUID.substr(27, 32);

                    for (const auto& ProjectAssetFile : std::filesystem::recursive_directory_iterator(UnityProjectPath)) {
                        if (ProjectAssetFile.is_directory() && skipFolders.find(ProjectAssetFile.path().filename().string()) != skipFolders.end()) {
                            continue;
                        }

                        std::ifstream _fileToCorrect(ProjectAssetFile.path());
                        if (!_fileToCorrect.is_open()) continue;

                        std::string fileToCorrectContents((std::istreambuf_iterator<char>(_fileToCorrect)), (std::istreambuf_iterator<char>()));
                        if (fileToCorrectContents.find(incorrectGuid) != std::string::npos) {
                            std::ofstream corrected(ProjectAssetFile.path());
                            if (!corrected.is_open()) continue;

                            corrected << ReplaceSubstring(fileToCorrectContents, incorrectGuid, CorrectGuid) << std::endl;
                            std::cout << "File " + ProjectAssetFile.path().filename().string() + " has been corrected\n";
                        }
                    }
                }
            }
        }
    }

    return 0;
}
