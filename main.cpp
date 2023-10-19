#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

const char* FILE_NAME = "intro.img";

constexpr int SECTOR_SIZE = 512;

struct BootSector {
	// BPB
	char ignore1[3];
	char oem[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fats;
	uint16_t max_root_dirs;
	uint16_t sector_count;
    char ignore2[1];
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t number_of_heads;
	uint32_t hidden_sectors;
	uint32_t total_sector_count_for_fat32;

	// FAT 12 Extended
	uint8_t drive_number;
	char ignore4[1];
	uint8_t boot_signature;
	uint32_t volume_id;
	char volume_label[11];
	char file_system_type[8];
	char boot_code[448];
	uint16_t bootable_partition_signature;
}__attribute__((packed));

struct DirectoryEntry {
	char file_name[8];
	char file_ext[3];

	// File attributes
	uint8_t read_only : 1;
	uint8_t hidden : 1;
	uint8_t system : 1;
	uint8_t volume_id : 1;
	uint8_t subdirectory : 1;
	uint8_t archive : 1;
	uint8_t device : 1;
	uint8_t unused : 1;

	uint8_t reserved;

	uint8_t create_time_high_res; // 10ms units, from 1-199
	uint16_t create_time;
	uint16_t create_date;
	uint16_t last_access_date;

	uint16_t ignore1;

	uint16_t last_write_time;
	uint16_t last_write_date;

	uint16_t first_cluster;
	uint32_t file_size; // in bytes

	bool has_long_name() {
		return (read_only | hidden | system | volume_id);
	}

	string_view get_file_name() {
		return string_view(file_name, sizeof(file_name));
	}

	string_view get_file_ext() {
		return string_view(file_ext, sizeof(file_ext));
	}
}__attribute__((packed));

struct FileSystemItem {
	DirectoryEntry* dirent;
	// int cluster = -1; // -1 for root directories

	bool is_subdir() {
		return dirent->subdirectory;
	}

	// bool is_root_dir() {
	// 	return cluster == -1;
	// }

	string name() {
		string result;
		const auto* name = dirent->file_name;
		const auto* ext = dirent->file_name;
		for (int i=0; i<8; i++) {
			if (name[i] == ' ')
				break;
			result += name[i];
		}
		if (not is_subdir()) {
			result += '.';
			for (int i=0; i<3; i++) {
				if (ext[i] == ' ')
					break;
				result += ext[i];
			}
		}
		return result;
	}
};

struct File : FileSystemItem {
	File(const vector<uint8_t>& cluster_data) {
		auto cluster = dirent->first_cluster;
		cluster -= 2; // first 2 clusters are ignored
		auto file_size = dirent->file_size;
		auto it_begin = cluster_data.begin()+SECTOR_SIZE*cluster;
		auto it_end = it_begin + file_size;
		contents = vector<uint8_t>(it_begin, it_end);
	}
	vector<uint8_t> contents;
};

struct Directory : FileSystemItem {
	vector<FileSystemItem*> dirs;
};

class Floppy {
public:
    bool read(ifstream& img) {
		/* READ BOOT SECTOR */

		img.read(reinterpret_cast<char*>(&boot), sizeof(boot));
		if (unsigned sig = boot.boot_signature; sig != 0x28 and sig != 0x29) {
			cerr << "ERROR: Signature is not valid: " << sig << endl;
			return false;
		}

		// skip reserved sectors 
		img.seekg((boot.reserved_sectors-1)*SECTOR_SIZE, std::ios_base::cur);


		/* READ FAT */

	    auto fat_table_size = boot.sectors_per_fat*SECTOR_SIZE;

		fat_data.resize(fat_table_size);
		img.read(reinterpret_cast<char*>(fat_data.data()), fat_data.size());

		// check if redundant FAT copies match the first copy
		{
			vector<uint8_t> fat_data_copy(fat_data.size());
			for (int i=0; i<boot.fats-1; i++) {
				img.read(reinterpret_cast<char*>(fat_data_copy.data()), fat_data.size());
				if (fat_data != fat_data_copy) {
					cerr << "FAT copy " << i+1 << " does not match the original." << endl;
					return false;
				}
			}
		}

		/* RECURSIVELY READ DIRECTORIES */
		{
			vector<DirectoryEntry> root_dirs_data(boot.max_root_dirs);
			auto root_dirs_size = sizeof(DirectoryEntry)*root_dirs.size();
			img.read(reinterpret_cast<char*>(root_dirs.data()), root_dirs_size);
			read_root_dirs(root_dirs_data);
		}

		/* READ CLUSTERS  */
		auto bytes_per_cluster = boot.sectors_per_cluster * SECTOR_SIZE;
		auto sectors_read = boot.reserved_sectors + boot.sectors_per_fat*boot.fats + (boot.max_root_dirs*sizeof(DirectoryEntry)/SECTOR_SIZE);
		auto cluster_count = (boot.sector_count - sectors_read) / boot.sectors_per_cluster;
		{
			vector<uint8_t> cluster_data(cluster_count*boot.sectors_per_cluster*SECTOR_SIZE);
			read_clusters(cluster_data, bytes_per_cluster);
		}
		return true;
	}
private:
	/* Read all the entries in the root directories and keep them
	 */
	void read_root_dirs(const vector<DirectoryEntry>& root_dirs_data) {
		for (size_t i=0; i<root_dirs_data.size(); i++) {
			const auto& d = root_dirs_data[i];
			if (d.file_name[0] == 0x0) {
				break;
			} else if ((uint8_t)d.file_name[0] == 0XE5) {
				// entry deleted but still available, we can skip this
				continue;
			}
		    // auto dir = new;
		}
	}

	/* Read all the clusters by doing a dfs from the root directories
	 */
	void read_clusters(const vector<uint8_t>& cluster_data, uint8_t bytes_per_cluster) {
	}
private:
	BootSector boot;
	vector<uint8_t> fat_data;
	vector<FileSystemItem*> root_dirs;
};

int main() {
	ifstream img(FILE_NAME);
    if (not img.is_open()) {
		cerr << "ERROR: Could not open file: " << FILE_NAME << endl;
		return 1;
	}

    Floppy floppy;
	floppy.read(img);

	// read boot sector
	return 0;
}
