// PAKTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <memory>

#include "FileFunctions.h"

struct pak_header {
	int header; // PAK 
	int version; // 256 or 1
	int files;
	int filesOffset;
	int stringSize;

	char pad[2028] = {};
};

struct pak_entry {
	int stringOffset;
	int offset;
	int size;
};

enum eModes {
	MODE_EXTRACT = 1,
	MODE_CREATE
};


int main(int argc, char* argv[])
{
	if (argc == 1) {
		std::cout << "Usage: paktool <file/folder>\n";
		return 1;
	}

	int mode = 0;

	std::string input = argv[argc - 1];

	if (!std::filesystem::exists(input))
	{
		std::cout << "ERROR: " << input << " does not exist!" << std::endl;
		return 1;
	}

	if (std::filesystem::is_directory(input))
		mode = MODE_CREATE;
	else
		mode = MODE_EXTRACT;


	if (mode == MODE_EXTRACT)
	{
		std::ifstream pFile(argv[argc - 1], std::ifstream::binary);

		if (!pFile)
		{
			std::cout << "ERROR: Could not open " << argv[argc - 1] << "!" << std::endl;
			return 1;
		}

		pak_header pak;
		pFile.read((char*)&pak, sizeof(pak_header));
		if (!(pak.header == 'PAK '))
		{
			std::cout << "ERROR: " << argv[argc - 1] << " is not a valid MK PAK archive!" << std::endl;
			return 1;
		}

		pFile.seekg(pak.filesOffset, pFile.beg);

		std::vector<pak_entry> Files;
		std::vector<std::string> Names;

		for (int i = 0; i < pak.files; i++)
		{
			pak_entry pak;
			pFile.read((char*)&pak, sizeof(pak_entry));
			Files.push_back(pak);
		}

		for (int i = 0; i < pak.files; i++)
		{
			pFile.seekg(pak.filesOffset + (sizeof(pak_entry) * pak.files), pFile.beg);
			pFile.seekg(Files[i].stringOffset, pFile.cur);

			std::string name;
			std::getline(pFile, name, '\0');

			if (name[0] == '\\')
				name = name.substr(1, name.length() - 1);
		
			Names.push_back(name);
		}


		for (int i = 0; i < pak.files; i++)
		{
			std::cout << "Processing: " << Names[i].c_str() << std::endl;
			pFile.seekg(Files[i].offset, pFile.beg);

			int dataSize = Files[i].size;
			std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(dataSize);
			pFile.read(dataBuff.get(), dataSize);

			std::filesystem::create_directories(splitString(Names[i], false));

			std::ofstream oFile(Names[i], std::ofstream::binary);
			oFile.write(dataBuff.get(), dataSize);

		}
		std::cout << "Finished." << std::endl;

	}

	if (mode == MODE_CREATE)
	{
		input = splitString(input, true);
		std::filesystem::path folder(input);

		if (!std::filesystem::exists(folder))
		{
			std::cout << "ERROR: Could not open directory: " << folder.string() << "!" << std::endl;
			return 1;
		}

		std::cout << "Processing folder: " << folder.string() << std::endl;


		std::vector<std::string> Names;
		std::vector<int> Sizes;

		for (const auto & file : std::filesystem::recursive_directory_iterator(folder))
		{
			if (!std::filesystem::is_directory(file))
			{
				std::string fileName = file.path().string();
				Names.push_back(fileName);
		
				Sizes.push_back(file.file_size());
			}
		}

		int totalSize = 0;
		int stringSize = 0;

		for (int i = 0; i < Sizes.size(); i++)
			totalSize += Sizes[i];

		for (int i = 0; i < Names.size(); i++)
		{
			int len = Names[i].length() + 1;
			// + 1 for slash thats added later
			if (!(Names[i][0] == '\\'))
				len += 1;

			int pad = makePad(len, 4);
#ifdef _DEBUG
			std::cout << Names[i].c_str() << " ";
			std::cout << len << " " << pad << std::endl;
#endif
			stringSize += pad;
		}
#ifdef _DEBUG
		std::cout << pak.stringSize << std::endl;
#endif
		pak_header pak;
		pak.header = 'PAK ';
		pak.version = 256;
		pak.filesOffset = sizeof(pak_header) + totalSize;
		pak.files = Names.size();
		pak.stringSize = stringSize;


		std::string str = folder.string();
		str += ".pak";

		std::ofstream oFile(str, std::ofstream::binary);

		oFile.write((char*)&pak, sizeof(pak_header));

		// data
		for (int i = 0; i < Names.size(); i++)
		{
			std::cout << "Processing: " << Names[i].c_str() << std::endl;
			std::ifstream pFile(Names[i], std::ifstream::binary);

			if (!pFile)
			{
				std::cout << "ERROR: Could not open " << Names[i].c_str() << "!" << std::endl;
				return 1;
			}

			int dataSize = getSizeToEnd(pFile);
			std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(dataSize);
			pFile.read(dataBuff.get(), dataSize);

			oFile.write(dataBuff.get(), dataSize);
		}
		std::cout << "File data saved." << std::endl;

		int baseOffset = sizeof(pak_header);
		int baseStringOffset = 0;


		for (int i = 0; i < Names.size(); i++)
		{
			pak_entry pak;
			pak.offset = baseOffset;
			pak.size = Sizes[i];
			pak.stringOffset = baseStringOffset;
			oFile.write((char*)&pak, sizeof(pak_entry));

			if (!(Names[i][0] == '\\'))
				Names[i].insert(0, "\\");

			baseOffset += Sizes[i];
			baseStringOffset += makePad(Names[i].length() + 1, 4);
		}

		std::cout << "File info saved." << std::endl;

		for (int i = 0; i < Names.size(); i++)
		{
			int len = Names[i].length() + 1;
			oFile.write(Names[i].c_str(), len);
			int pad = makePad(len, 4);
			int size = pad - len;

			if (size > 0)
			{
				std::unique_ptr<char[]> dummy = std::make_unique<char[]>(size);
				oFile.write(dummy.get(), size);
			}

			

		}
		std::cout << "File names saved." << std::endl;

		std::cout << "Saved as " << str.c_str() << std::endl;
		std::cout << "Finished." << std::endl;


	}
}
