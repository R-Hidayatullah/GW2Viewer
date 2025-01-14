#ifndef DAT_FILE_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// Constants
constexpr size_t DAT_MAGIC_NUMBER = 3;
constexpr size_t MFT_MAGIC_NUMBER = 4;
constexpr size_t MFT_ENTRY_INDEX_NUM = 1;

class DatFile {
public:
	// Nested structures
	struct DatHeader {
		uint8_t version;
		char identifier[DAT_MAGIC_NUMBER];
		uint32_t header_size;
		uint32_t unknown_field;
		uint32_t chunk_size;
		uint32_t crc;
		uint32_t unknown_field_2;
		uint64_t mft_offset;
		uint32_t mft_size;
		uint32_t flag;

		DatHeader() : version(0), header_size(0), unknown_field(0), chunk_size(0), crc(0),
			unknown_field_2(0), mft_offset(0), mft_size(0), flag(0) {
			std::memset(identifier, 0, DAT_MAGIC_NUMBER);
		}
	};

	struct MftHeader {
		char identifier[MFT_MAGIC_NUMBER];
		uint64_t unknown_field;
		uint32_t mft_entry_size;
		uint32_t unknown_field_2;
		uint32_t unknown_field_3;

		MftHeader() : unknown_field(0), mft_entry_size(0), unknown_field_2(0), unknown_field_3(0) {
			std::memset(identifier, 0, MFT_MAGIC_NUMBER);
		}
	};

	struct MftData {
		uint64_t offset;
		uint32_t size;
		uint16_t compression_flag;
		uint16_t entry_flag;
		uint32_t counter;
		uint32_t crc;
		uint32_t uncompressed_size;
		std::vector<std::pair<uint64_t, uint32_t>> crc_32c_data;

		MftData() : offset(0), size(0), compression_flag(0), entry_flag(0), counter(0), crc(0),
			uncompressed_size(0) {
		}
	};

	struct MftIndexData {
		uint32_t file_id;
		uint32_t base_id;

		MftIndexData() : file_id(0), base_id(0) {}
	};

	// Constructor
	DatFile(const std::string& file_path) : filename(file_path), file_size(0) {
		load();
	}

	// Public methods
	void load() {
		validateFileExtension();
		openFile();
		readDatHeader();
		readMftHeader();
		readMftData();
		readMftIndexData();
	}

	void printSummary() const {
		// Print DAT file header summary
		std::cout << "DAT File: " << filename << "\n"
			<< "File Size: " << file_size << " bytes\n"
			<< "Version: " << static_cast<int>(dat_header.version) << "\n"
			<< "Chunk Size: " << dat_header.chunk_size << " bytes\n"
			<< "MFT Offset: " << dat_header.mft_offset << "\n"
			<< "MFT Size: " << dat_header.mft_size << " bytes\n";

		// Print MFT header summary
		std::cout << "\nMFT Header:\n"
			<< "Identifier: " << std::string(mft_header.identifier, mft_header.identifier + MFT_MAGIC_NUMBER) << "\n"
			<< "Unknown Field: " << mft_header.unknown_field << "\n"
			<< "Entry Count: " << mft_header.mft_entry_size << "\n"
			<< "Unknown Field 2: " << mft_header.unknown_field_2 << "\n"
			<< "Unknown Field 3: " << mft_header.unknown_field_3 << "\n";
	}

	const DatHeader& getHeader() const {
		return dat_header;
	}

	const MftHeader& getMftHeader() const {
		return mft_header;
	}

	const std::string& getFilename() const {
		return filename;
	}

	uint64_t getFileSize() const {
		return file_size;
	}

	const std::vector<DatFile::MftData>& getMftData() const {
		return mft_data;
	}

	// Function to read compressed data
	std::vector<uint8_t> readCompressedData(const MftData& entry) {
		std::vector<uint8_t> compressed_data(entry.size);

		// Seek to the specified offset
		file.seekg(entry.offset);
		if (!file) {
			throw std::runtime_error("Failed to seek to offset: " + std::to_string(entry.offset) +
				" in file: " + filename);
		}


		// Read data into the buffer
		file.read(reinterpret_cast<char*>(compressed_data.data()), entry.size);

		// Check if the read was successful
		if (file.gcount() != entry.size) {
			throw std::runtime_error("Failed to read the full size from file: " + filename + " in offset: " + std::to_string(entry.offset));
		}

		return compressed_data;
	}

	std::vector<uint32_t> findMftData(uint32_t data_id, bool is_base_id) {
		std::unordered_set<uint32_t> data_found = {};

		// Length of the number to find
		uint32_t divisor = 1;
		uint32_t temp = data_id;
		while (temp > 0)
		{
			divisor *= 10;
			temp /= 10;
		}

		// Search for the number
		for (uint32_t i = 0; i < mft_index_data.size(); i++)
		{
			if (is_base_id)
			{
				uint32_t num = mft_index_data[i].base_id;
				while (num >= data_id) // Only check if num is large enough
				{
					if (num % divisor == data_id) // Match the substring
					{
						data_found.insert(mft_index_data[i].base_id);
						break;
					}
					num /= 10; // Remove the last digit
				}
			}
			else {
				uint32_t num = mft_index_data[i].file_id;
				while (num >= data_id) // Only check if num is large enough
				{
					if (num % divisor == data_id) // Match the substring
					{
						data_found.insert(mft_index_data[i].base_id);
						break;
					}
					num /= 10; // Remove the last digit
				}
			}

		}

		return std::vector<uint32_t>(data_found.begin(), data_found.end());
	}

private:
	// Member variables
	std::string filename;
	uint64_t file_size;
	DatHeader dat_header;
	MftHeader mft_header;
	std::vector<MftData> mft_data;
	std::vector<MftIndexData> mft_index_data;
	std::ifstream file;

	// Private methods
	void validateFileExtension() {
		if (filename.substr(filename.find_last_of(".") + 1) != "dat") {
			throw std::invalid_argument("Invalid file extension. Expected '.dat'.");
		}
	}

	void openFile() {
		file.open(filename, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file: " + filename);
		}
		file_size = file.tellg();
		file.seekg(0, std::ios::beg);
	}

	void readDatHeader() {
		file.read(reinterpret_cast<char*>(&dat_header.version), sizeof(dat_header.version));
		file.read(dat_header.identifier, DAT_MAGIC_NUMBER);
		file.read(reinterpret_cast<char*>(&dat_header.header_size), sizeof(dat_header.header_size));
		file.read(reinterpret_cast<char*>(&dat_header.unknown_field), sizeof(dat_header.unknown_field));
		file.read(reinterpret_cast<char*>(&dat_header.chunk_size), sizeof(dat_header.chunk_size));
		file.read(reinterpret_cast<char*>(&dat_header.crc), sizeof(dat_header.crc));
		file.read(reinterpret_cast<char*>(&dat_header.unknown_field_2), sizeof(dat_header.unknown_field_2));
		file.read(reinterpret_cast<char*>(&dat_header.mft_offset), sizeof(dat_header.mft_offset));
		file.read(reinterpret_cast<char*>(&dat_header.mft_size), sizeof(dat_header.mft_size));
		file.read(reinterpret_cast<char*>(&dat_header.flag), sizeof(dat_header.flag));
	}

	void readMftHeader() {
		file.seekg(dat_header.mft_offset, std::ios::beg);
		file.read(mft_header.identifier, MFT_MAGIC_NUMBER);
		file.read(reinterpret_cast<char*>(&mft_header.unknown_field), sizeof(mft_header.unknown_field));
		file.read(reinterpret_cast<char*>(&mft_header.mft_entry_size), sizeof(mft_header.mft_entry_size));
		file.read(reinterpret_cast<char*>(&mft_header.unknown_field_2), sizeof(mft_header.unknown_field_2));
		file.read(reinterpret_cast<char*>(&mft_header.unknown_field_3), sizeof(mft_header.unknown_field_3));
		mft_header.mft_entry_size -= 1; // Adjust size based on data format
	}

	void readMftData() {
		for (size_t i = 0; i < mft_header.mft_entry_size; ++i) {
			MftData entry;
			file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
			file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
			file.read(reinterpret_cast<char*>(&entry.compression_flag), sizeof(entry.compression_flag));
			file.read(reinterpret_cast<char*>(&entry.entry_flag), sizeof(entry.entry_flag));
			file.read(reinterpret_cast<char*>(&entry.counter), sizeof(entry.counter));
			file.read(reinterpret_cast<char*>(&entry.crc), sizeof(entry.crc));
			mft_data.push_back(entry);
		}
	}

	void readMftIndexData() {
		if (MFT_ENTRY_INDEX_NUM >= mft_data.size()) {
			return;
		}

		const auto& entry = mft_data[MFT_ENTRY_INDEX_NUM];
		size_t num_entries = entry.size / sizeof(MftIndexData);
		file.seekg(entry.offset, std::ios::beg);

		for (size_t i = 0; i < num_entries; ++i) {
			MftIndexData index_entry;
			file.read(reinterpret_cast<char*>(&index_entry.file_id), sizeof(index_entry.file_id));
			file.read(reinterpret_cast<char*>(&index_entry.base_id), sizeof(index_entry.base_id));
			mft_index_data.push_back(index_entry);
		}
	}
};


#endif // !DAT_FILE_H
