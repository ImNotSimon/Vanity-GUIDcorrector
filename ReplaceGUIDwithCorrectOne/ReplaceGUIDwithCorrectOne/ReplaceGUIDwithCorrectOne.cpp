#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <Windows.h>
#include <vector>
#include <map>

std::string GetGuidFromMeta(const std::string& path)
{
    std::ifstream fileStream(path);
    std::string content((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    fileStream.close();

    int guidIndex = content.find("guid: ");
    if (guidIndex == std::string::npos)
        return "";

    return content.substr(guidIndex + 6, 32);
}

void ChangeMetaGuid(const std::string& metaPath, const std::string newGuid)
{
    std::ifstream fileStream(metaPath);
    std::string content((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    fileStream.close();

    int guidIndex = content.find("guid: ");
    if (guidIndex == std::string::npos)
        return;

    guidIndex += 6;

    std::fstream outputStream(metaPath, std::ios::in | std::ios::out);
    outputStream.seekp(guidIndex);
    outputStream << newGuid;
    outputStream.close();
}

bool ends_with(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main(int argc, char* argv[])
{
    #ifdef LOG
    std::fstream out("log.txt", std::ios::out);
    #else
    std::ostream& out = std::cout;
    #endif

    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <IncorrectGUIDsPath> <CorrectGUIDsPath> <UnityProjectPath>\n";
        return 1;
    }

    std::string IncorrectGUIDsPath = argv[1];
    std::string CorrectGUIDsPath = argv[2];
    std::string UnityProjectPath = argv[3];

    std::filesystem::path path1 = IncorrectGUIDsPath;
    std::filesystem::path path2 = CorrectGUIDsPath;
    std::filesystem::path path3 = UnityProjectPath;

    if (!std::filesystem::exists(path1) || !std::filesystem::exists(path2) || !std::filesystem::exists(path3))
    {
        std::cerr << "One or more paths are invalid. Please double check and try again.\n";
        return 1;
    }

    out << "Creating GUID map...\n";
    std::map<std::string, std::string> fileNameToCorrectGuid;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(CorrectGUIDsPath))
    {
        if (!ends_with(entry.path().string(), ".cs.meta"))
            continue;
        
        std::string guid = GetGuidFromMeta(entry.path().string());
        if (guid.length() == 0)
            continue;

        fileNameToCorrectGuid[entry.path().filename().string()] = guid;
    }
    
    std::map<std::string, std::string> incorrectGuidToCorrectGuid;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(IncorrectGUIDsPath))
    {
        if (!ends_with(entry.path().string(), ".cs.meta"))
            continue;

        std::string incorrectGuid = GetGuidFromMeta(entry.path().string());
        if (incorrectGuid.length() == 0)
            continue;

        auto correctGuidEntry = fileNameToCorrectGuid.find(entry.path().filename().string());
        if (correctGuidEntry == fileNameToCorrectGuid.end())
            continue;
        
        incorrectGuidToCorrectGuid[incorrectGuid] = correctGuidEntry->second;
        out << incorrectGuid << " => " << correctGuidEntry->second << "\n";
    }

    out << "Fixing files...\n";

    char tempChar;
    char guidCheck[8];
    guidCheck[7] = 0;
    char guid[33];
    guid[32] = 0;

    for (const auto& ProjectAssetFile : std::filesystem::recursive_directory_iterator(UnityProjectPath))
    {
        if (ProjectAssetFile.is_directory())
            continue;
        
        out << "Processing " << ProjectAssetFile.path().string() << "...\n";

        // Special operation for meta files
        if (ProjectAssetFile.path().extension() == ".meta")
        {
            std::string metaGuid = GetGuidFromMeta(ProjectAssetFile.path().string());
            if (metaGuid.length() == 0)
                continue;
            
            auto correctMetaGuid = incorrectGuidToCorrectGuid.find(metaGuid);
            if (correctMetaGuid == incorrectGuidToCorrectGuid.end())
                continue;

            ChangeMetaGuid(ProjectAssetFile.path().string(), correctMetaGuid->second);
            continue;
        }

        std::fstream assetStream(ProjectAssetFile.path().string(), std::ios::in | std::ios::out);
        if (!assetStream.is_open())
        {
            out << "   COULD NOT OPEN THE FILE, SKIPPING\n";
            continue;
        }

        char header[6];
        header[5] = 0;

        assetStream.read(header, 5);
        if (std::string(header) != "%YAML")
        {
            out << "   NON SERIALIZED FILE, SKIPPING\n";
            continue;
        }

        while (!assetStream.eof())
        {
            // Sample object reference: {fileID: 10905, guid: 0000000000000000f000000000000000, type: 0}

            // Read until '{'
            assetStream.read(&tempChar, 1);
            if (tempChar == '{')
            {
                while (!assetStream.eof())
                {
                    // Read until comma or end of reference
                    assetStream.read(&tempChar, 1);
                    if (tempChar == '}')
                        break;

                    if (tempChar == ',')
                    {
                        assetStream.read(guidCheck, 7);
                        if (std::string(guidCheck) == " guid: ")
                        {
                            assetStream.read(guid, 32);
                            std::string currentGuid(guid);
                            auto correctGuid = incorrectGuidToCorrectGuid.find(currentGuid);

                            if (correctGuid != incorrectGuidToCorrectGuid.end())
                            {
                                assetStream.seekg(-32, std::ios::cur);
                                assetStream << correctGuid->second;
                            }
                        }

                        break;
                    }
                }
            }
        }

        assetStream.close();
    }

    return 0;
}
