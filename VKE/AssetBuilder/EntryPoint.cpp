/*
	This project is to help move asset from engine path to build path
*/
#include "Core.h"
#include <string>
#include <windows.h>
#include <vector>

#define SRC_PATH std::string("/Engine/Content/Shaders/")
#define DEST_PATH_PREFIX std::string("Game/")

#define DEST_PATH_SUFFIX std::string("Content/Shaders/")

bool CreateDirectory(std::string newFolder)
{
	if (!(CreateDirectory(newFolder.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS))
	{
		// Failed to create directory.
		printf("Fail to create directory %s\n, error: %s", newFolder.c_str(), (GetLastError() == ERROR_PATH_NOT_FOUND) ? "Path not found" : "Folder already existed");
		return false;
	}
	return true;
}

bool CopyFile(const std::string& iFileName)
{
	std::string sourceFile = SOLUTION_DIR + SRC_PATH + iFileName;
	std::string destFilePath = SOLUTION_DIR + DEST_PATH_PREFIX;// + _PLATFORM + _CONFIGURATION;

	std::vector<std::string> Folders = { "Content", "Shaders" };
	size_t lastFolderIdx = 0;
	for (size_t i = 0; i < iFileName.length(); ++i)
	{
		if (iFileName[i] == '/')
		{
			auto substr = iFileName.substr(lastFolderIdx, i - lastFolderIdx);
			Folders.push_back(substr);
			lastFolderIdx = i + 1;
		}
	}
	for (size_t i = 0; i < Folders.size(); ++i)
	{
		destFilePath = destFilePath + Folders[i] + "/";
		if (!CreateDirectory(destFilePath))
		{
			return false;
		}
	}
	destFilePath += (lastFolderIdx == 0?iFileName : iFileName.substr(lastFolderIdx, iFileName.length() - lastFolderIdx));
	// Copy files
	if (CopyFile(sourceFile.c_str(), destFilePath.c_str(), FALSE) == FALSE)
	{
		printf("[Error] Fail to copy file: %s\n", iFileName.c_str());
		return false;
	}
	else
	{
		printf("File copied: %s\n", iFileName.c_str());
	}
	return true;
	
}

int32 main(int argc, char *argv[])
{
	for (int32 i = 1; i < argc; ++i)
	{
		CopyFile(argv[i]);
	}
	
	return 0;
}