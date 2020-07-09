/*
	This project is to help move asset from engine path to build path
*/
#include <string>
#include <windows.h>


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

	const static char* Folders[] = { "Content", "Shaders" };
	for (int i = 0; i < 2; ++i)
	{
		destFilePath = destFilePath + Folders[i] + "/";
		if (!CreateDirectory(destFilePath))
		{
			return false;
		}
	}
	destFilePath += iFileName;
	// Copy files
	if (CopyFile(sourceFile.c_str(), destFilePath.c_str(), FALSE) == FALSE)
	{
		printf("Fail to copy file: %s\n", iFileName.c_str());
		return false;
	}
	else
	{
		printf("File copied: %s\n", iFileName.c_str());
	}
	return true;
	
}

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		CopyFile(argv[i]);
	}
	
	return 0;
}