/**************************************************************************/
/*  dir_access_unix.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "dir_access_unix.h"

#if defined(UNIX_ENABLED)

#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/templates/list.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/statfs.h>
#endif
#include <sys/statvfs.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#if __has_include(<mntent.h>)
#include <mntent.h>
#endif

Error DirAccessUnix::list_dir_begin() {
	list_dir_end(); //close any previous dir opening!

	//char real_current_dir_name[2048]; //is this enough?!
	//getcwd(real_current_dir_name,2048);
	//chdir(current_path.utf8().get_data());
	dir_stream = opendir(current_dir.utf8().get_data());
	//chdir(real_current_dir_name);
	if (!dir_stream) {
		return ERR_CANT_OPEN; //error!
	}

	return OK;
}

bool DirAccessUnix::file_exists(String p_file) {
	GLOBAL_LOCK_FUNCTION

	if (p_file.is_relative_path()) {
		p_file = current_dir.path_join(p_file);
	}

	p_file = fix_path(p_file);

	struct stat flags = {};
	bool success = (stat(p_file.utf8().get_data(), &flags) == 0);

	if (success && S_ISDIR(flags.st_mode)) {
		success = false;
	}

	return success;
}

bool DirAccessUnix::dir_exists(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	if (p_dir.is_relative_path()) {
		p_dir = get_current_dir().path_join(p_dir);
	}

	p_dir = fix_path(p_dir);

	struct stat flags = {};
	bool success = (stat(p_dir.utf8().get_data(), &flags) == 0);

	return (success && S_ISDIR(flags.st_mode));
}

bool DirAccessUnix::is_readable(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	if (p_dir.is_relative_path()) {
		p_dir = get_current_dir().path_join(p_dir);
	}

	p_dir = fix_path(p_dir);
	return (access(p_dir.utf8().get_data(), R_OK) == 0);
}

bool DirAccessUnix::is_writable(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	if (p_dir.is_relative_path()) {
		p_dir = get_current_dir().path_join(p_dir);
	}

	p_dir = fix_path(p_dir);
	return (access(p_dir.utf8().get_data(), W_OK) == 0);
}

uint64_t DirAccessUnix::get_modified_time(String p_file) {
	if (p_file.is_relative_path()) {
		p_file = current_dir.path_join(p_file);
	}

	p_file = fix_path(p_file);

	struct stat flags = {};
	bool success = (stat(p_file.utf8().get_data(), &flags) == 0);

	if (success) {
		return flags.st_mtime;
	} else {
		ERR_FAIL_V(0);
	}
	return 0;
}

String DirAccessUnix::get_next() {
	if (!dir_stream) {
		return "";
	}

	dirent *entry = readdir(dir_stream);

	if (entry == nullptr) {
		list_dir_end();
		return "";
	}

	String fname = fix_unicode_name(entry->d_name);

	// Look at d_type to determine if the entry is a directory, unless
	// its type is unknown (the file system does not support it) or if
	// the type is a link, in that case we want to resolve the link to
	// known if it points to a directory. stat() will resolve the link
	// for us.
	if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
		String f = current_dir.path_join(fname);

		struct stat flags = {};
		if (stat(f.utf8().get_data(), &flags) == 0) {
			_cisdir = S_ISDIR(flags.st_mode);
		} else {
			_cisdir = false;
		}
	} else {
		_cisdir = (entry->d_type == DT_DIR);
	}

	_cishidden = is_hidden(fname);

	return fname;
}

bool DirAccessUnix::current_is_dir() const {
	return _cisdir;
}

bool DirAccessUnix::current_is_hidden() const {
	return _cishidden;
}

void DirAccessUnix::list_dir_end() {
	if (dir_stream) {
		closedir(dir_stream);
	}
	dir_stream = nullptr;
	_cisdir = false;
}

#if __has_include(<mntent.h>) && defined(LINUXBSD_ENABLED)
static bool _filter_drive(struct mntent *mnt) {
	// Ignore devices that don't point to /dev
	if (strncmp(mnt->mnt_fsname, "/dev", 4) != 0) {
		return false;
	}

	// Accept devices mounted at common locations
	if (strncmp(mnt->mnt_dir, "/media", 6) == 0 ||
			strncmp(mnt->mnt_dir, "/mnt", 4) == 0 ||
			strncmp(mnt->mnt_dir, "/home", 5) == 0 ||
			strncmp(mnt->mnt_dir, "/run/media", 10) == 0) {
		return true;
	}

	// Ignore everything else
	return false;
}
#endif

static void _get_drives(List<String> *list) {
	// Add root.
	list->push_back("/");

#if __has_include(<mntent.h>) && defined(LINUXBSD_ENABLED)
	// Check /etc/mtab for the list of mounted partitions.
	FILE *mtab = setmntent("/etc/mtab", "r");
	if (mtab) {
		struct mntent mnt;
		char strings[4096];

		while (getmntent_r(mtab, &mnt, strings, sizeof(strings))) {
			if (mnt.mnt_dir != nullptr && _filter_drive(&mnt)) {
				// Avoid duplicates
				String name = String::utf8(mnt.mnt_dir);
				if (!list->find(name)) {
					list->push_back(name);
				}
			}
		}

		endmntent(mtab);
	}
#endif

	// Add $HOME.
	const char *home = getenv("HOME");
	if (home) {
		// Only add if it's not a duplicate
		String home_name = String::utf8(home);
		if (!list->find(home_name)) {
			list->push_back(home_name);
		}

		// Check GTK+3 bookmarks for both XDG locations (Documents, Downloads, etc.)
		// and potential user-defined bookmarks.
		char path[1024];
		snprintf(path, 1024, "%s/.config/gtk-3.0/bookmarks", home);
		FILE *fd = fopen(path, "r");
		if (fd) {
			char string[1024];
			while (fgets(string, 1024, fd)) {
				// Parse only file:// links
				if (strncmp(string, "file://", 7) == 0) {
					// Strip any unwanted edges on the strings and push_back if it's not a duplicate.
					String fpath = String::utf8(string + 7).strip_edges().split_spaces()[0].uri_file_decode();
					if (!list->find(fpath)) {
						list->push_back(fpath);
					}
				}
			}

			fclose(fd);
		}

		// Add Desktop dir.
		String dpath = OS::get_singleton()->get_system_dir(OS::SystemDir::SYSTEM_DIR_DESKTOP);
		if (dpath.length() > 0 && !list->find(dpath)) {
			list->push_back(dpath);
		}
	}

	list->sort();
}

int DirAccessUnix::get_drive_count() {
	List<String> list;
	_get_drives(&list);

	return list.size();
}

String DirAccessUnix::get_drive(int p_drive) {
	List<String> list;
	_get_drives(&list);

	ERR_FAIL_INDEX_V(p_drive, list.size(), "");

	return list.get(p_drive);
}

int DirAccessUnix::get_current_drive() {
	int drive = 0;
	int max_length = -1;
	const String path = get_current_dir().to_lower();
	for (int i = 0; i < get_drive_count(); i++) {
		const String d = get_drive(i).to_lower();
		if (max_length < d.length() && path.begins_with(d)) {
			max_length = d.length();
			drive = i;
		}
	}
	return drive;
}

bool DirAccessUnix::drives_are_shortcuts() {
	return true;
}

Error DirAccessUnix::make_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	if (p_dir.is_relative_path()) {
		p_dir = get_current_dir().path_join(p_dir);
	}

	p_dir = fix_path(p_dir);

	bool success = (mkdir(p_dir.utf8().get_data(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0);
	int err = errno;

	if (success) {
		return OK;
	}

	if (err == EEXIST) {
		return ERR_ALREADY_EXISTS;
	}

	return ERR_CANT_CREATE;
}

Error DirAccessUnix::change_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	p_dir = fix_path(p_dir);

	// prev_dir is the directory we are changing out of
	String prev_dir;
	char real_current_dir_name[2048];
	ERR_FAIL_NULL_V(getcwd(real_current_dir_name, 2048), ERR_BUG);
	if (prev_dir.append_utf8(real_current_dir_name) != OK) {
		prev_dir = real_current_dir_name; //no utf8, maybe latin?
	}

	// try_dir is the directory we are trying to change into
	String try_dir = "";
	if (p_dir.is_relative_path()) {
		String next_dir = current_dir.path_join(p_dir);
		next_dir = next_dir.simplify_path();
		try_dir = next_dir;
	} else {
		try_dir = p_dir;
	}

	bool worked = (chdir(try_dir.utf8().get_data()) == 0); // we can only give this utf8
	if (!worked) {
		return ERR_INVALID_PARAMETER;
	}

	String base = _get_root_path();
	if (!base.is_empty() && !try_dir.begins_with(base)) {
		ERR_FAIL_NULL_V(getcwd(real_current_dir_name, 2048), ERR_BUG);
		String new_dir;
		new_dir.append_utf8(real_current_dir_name);

		if (!new_dir.begins_with(base)) {
			try_dir = current_dir; //revert
		}
	}

	// the directory exists, so set current_dir to try_dir
	current_dir = try_dir;
	ERR_FAIL_COND_V(chdir(prev_dir.utf8().get_data()) != 0, ERR_BUG);
	return OK;
}

String DirAccessUnix::get_current_dir(bool p_include_drive) const {
	String base = _get_root_path();
	if (!base.is_empty()) {
		String bd = current_dir.replace_first(base, "");
		if (bd.begins_with("/")) {
			return _get_root_string() + bd.substr(1);
		} else {
			return _get_root_string() + bd;
		}
	}
	return current_dir;
}

Error DirAccessUnix::rename(String p_path, String p_new_path) {
	if (p_path.is_relative_path()) {
		p_path = get_current_dir().path_join(p_path);
	}

	p_path = fix_path(p_path);
	if (p_path.ends_with("/")) {
		p_path = p_path.left(-1);
	}

	if (p_new_path.is_relative_path()) {
		p_new_path = get_current_dir().path_join(p_new_path);
	}

	p_new_path = fix_path(p_new_path);
	if (p_new_path.ends_with("/")) {
		p_new_path = p_new_path.left(-1);
	}

	int res = ::rename(p_path.utf8().get_data(), p_new_path.utf8().get_data());
	if (res != 0 && errno == EXDEV) { // Cross-device move, use copy and remove.
		Error err = OK;
		err = copy(p_path, p_new_path);
		if (err != OK) {
			return err;
		}
		return remove(p_path);
	} else {
		return (res == 0) ? OK : FAILED;
	}
}

Error DirAccessUnix::remove(String p_path) {
	if (p_path.is_relative_path()) {
		p_path = get_current_dir().path_join(p_path);
	}

	p_path = fix_path(p_path);
	if (p_path.ends_with("/")) {
		p_path = p_path.left(-1);
	}

	struct stat flags = {};
	if ((lstat(p_path.utf8().get_data(), &flags) != 0)) {
		return FAILED;
	}

	int err;
	if (S_ISDIR(flags.st_mode) && !is_link(p_path)) {
		err = ::rmdir(p_path.utf8().get_data());
	} else {
		err = ::unlink(p_path.utf8().get_data());
	}
	if (err != 0) {
		return FAILED;
	}
	if (remove_notification_func != nullptr) {
		remove_notification_func(p_path);
	}
	return OK;
}

bool DirAccessUnix::is_link(String p_file) {
	if (p_file.is_relative_path()) {
		p_file = get_current_dir().path_join(p_file);
	}

	p_file = fix_path(p_file);
	if (p_file.ends_with("/")) {
		p_file = p_file.left(-1);
	}

	struct stat flags = {};
	if ((lstat(p_file.utf8().get_data(), &flags) != 0)) {
		return false;
	}

	return S_ISLNK(flags.st_mode);
}

String DirAccessUnix::read_link(String p_file) {
	if (p_file.is_relative_path()) {
		p_file = get_current_dir().path_join(p_file);
	}

	p_file = fix_path(p_file);
	if (p_file.ends_with("/")) {
		p_file = p_file.left(-1);
	}

	char buf[256];
	memset(buf, 0, 256);
	ssize_t len = readlink(p_file.utf8().get_data(), buf, sizeof(buf));
	String link;
	if (len > 0) {
		link.append_utf8(buf, len);
	}
	return link;
}

Error DirAccessUnix::create_link(String p_source, String p_target) {
	if (p_target.is_relative_path()) {
		p_target = get_current_dir().path_join(p_target);
	}

	p_source = fix_path(p_source);
	p_target = fix_path(p_target);

	if (symlink(p_source.utf8().get_data(), p_target.utf8().get_data()) == 0) {
		return OK;
	} else {
		return FAILED;
	}
}

uint64_t DirAccessUnix::get_space_left() {
	struct statvfs vfs;
	if (statvfs(current_dir.utf8().get_data(), &vfs) != 0) {
		return 0;
	}

	return (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
}

String DirAccessUnix::get_filesystem_type() const {
#ifdef __linux__
	struct statfs fs;
	if (statfs(current_dir.utf8().get_data(), &fs) != 0) {
		return "";
	}
	switch (static_cast<unsigned int>(fs.f_type)) {
		case 0x0000adf5:
			return "ADFS";
		case 0x0000adff:
			return "AFFS";
		case 0x5346414f:
			return "AFS";
		case 0x00000187:
			return "AUTOFS";
		case 0x00c36400:
			return "CEPH";
		case 0x73757245:
			return "CODA";
		case 0x28cd3d45:
			return "CRAMFS";
		case 0x453dcd28:
			return "CRAMFS";
		case 0x64626720:
			return "DEBUGFS";
		case 0x73636673:
			return "SECURITYFS";
		case 0xf97cff8c:
			return "SELINUX";
		case 0x43415d53:
			return "SMACK";
		case 0x858458f6:
			return "RAMFS";
		case 0x01021994:
			return "TMPFS";
		case 0x958458f6:
			return "HUGETLBFS";
		case 0x73717368:
			return "SQUASHFS";
		case 0x0000f15f:
			return "ECRYPTFS";
		case 0x00414a53:
			return "EFS";
		case 0xe0f5e1e2:
			return "EROFS";
		case 0x0000ef53:
			return "EXTFS";
		case 0xabba1974:
			return "XENFS";
		case 0x9123683e:
			return "BTRFS";
		case 0x00003434:
			return "NILFS";
		case 0xf2f52010:
			return "F2FS";
		case 0xf995e849:
			return "HPFS";
		case 0x00009660:
			return "ISOFS";
		case 0x000072b6:
			return "JFFS2";
		case 0x58465342:
			return "XFS";
		case 0x6165676c:
			return "PSTOREFS";
		case 0xde5e81e4:
			return "EFIVARFS";
		case 0x00c0ffee:
			return "HOSTFS";
		case 0x794c7630:
			return "OVERLAYFS";
		case 0x65735546:
			return "FUSE";
		case 0xca451a4e:
			return "BCACHEFS";
		case 0x00004d44:
			return "FAT32";
		case 0x2011bab0:
			return "EXFAT";
		case 0x0000564c:
			return "NCP";
		case 0x00006969:
			return "NFS";
		case 0x7461636f:
			return "OCFS2";
		case 0x00009fa1:
			return "OPENPROM";
		case 0x0000002f:
			return "QNX4";
		case 0x68191122:
			return "QNX6";
		case 0x6b414653:
			return "AFS";
		case 0x52654973:
			return "REISERFS";
		case 0x0000517b:
			return "SMB";
		case 0xff534d42:
			return "CIFS";
		case 0x0027e0eb:
			return "CGROUP";
		case 0x63677270:
			return "CGROUP2";
		case 0x07655821:
			return "RDTGROUP";
		case 0x74726163:
			return "TRACEFS";
		case 0x01021997:
			return "V9FS";
		case 0x62646576:
			return "BDEVFS";
		case 0x64646178:
			return "DAXFS";
		case 0x42494e4d:
			return "BINFMTFS";
		case 0x00001cd1:
			return "DEVPTS";
		case 0x6c6f6f70:
			return "BINDERFS";
		case 0x0bad1dea:
			return "FUTEXFS";
		case 0x50495045:
			return "PIPEFS";
		case 0x00009fa0:
			return "PROC";
		case 0x534f434b:
			return "SOCKFS";
		case 0x62656572:
			return "SYSFS";
		case 0x00009fa2:
			return "USBDEVICE";
		case 0x11307854:
			return "MTD_INODE";
		case 0x09041934:
			return "ANON_INODE";
		case 0x73727279:
			return "BTRFS";
		case 0x6e736673:
			return "NSFS";
		case 0xcafe4a11:
			return "BPF_FS";
		case 0x5a3c69f0:
			return "AAFS";
		case 0x5a4f4653:
			return "ZONEFS";
		case 0x15013346:
			return "UDF";
		case 0x444d4142:
			return "DMA_BUF";
		case 0x454d444d:
			return "DEVMEM";
		case 0x5345434d:
			return "SECRETMEM";
		case 0x50494446:
			return "PID_FS";
		default:
			return "";
	}
#else
	return ""; //TODO this should be implemented
#endif
}

bool DirAccessUnix::is_hidden(const String &p_name) {
	return p_name != "." && p_name != ".." && p_name.begins_with(".");
}

bool DirAccessUnix::is_case_sensitive(const String &p_path) const {
#if defined(LINUXBSD_ENABLED)
	String f = p_path;
	if (!f.is_absolute_path()) {
		f = get_current_dir().path_join(f);
	}
	f = fix_path(f);

	int fd = ::open(f.utf8().get_data(), O_RDONLY | O_NONBLOCK);
	if (fd) {
		long flags = 0;
		if (ioctl(fd, _IOR('f', 1, long), &flags) >= 0) {
			::close(fd);
			return !(flags & 0x40000000 /* FS_CASEFOLD_FL */);
		}
		::close(fd);
	}
#endif
	return true;
}

bool DirAccessUnix::is_equivalent(const String &p_path_a, const String &p_path_b) const {
	String f1 = fix_path(p_path_a);
	struct stat st1 = {};
	int err = stat(f1.utf8().get_data(), &st1);
	if (err) {
		return DirAccess::is_equivalent(p_path_a, p_path_b);
	}

	String f2 = fix_path(p_path_b);
	struct stat st2 = {};
	err = stat(f2.utf8().get_data(), &st2);
	if (err) {
		return DirAccess::is_equivalent(p_path_a, p_path_b);
	}

	return (st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino);
}

DirAccessUnix::DirAccessUnix() {
	dir_stream = nullptr;
	_cisdir = false;

	/* determine drive count */

	// set current directory to an absolute path of the current directory
	char real_current_dir_name[2048];
	ERR_FAIL_NULL(getcwd(real_current_dir_name, 2048));
	current_dir.clear();
	if (current_dir.append_utf8(real_current_dir_name) != OK) {
		current_dir = real_current_dir_name;
	}

	change_dir(current_dir);
}

DirAccessUnix::RemoveNotificationFunc DirAccessUnix::remove_notification_func = nullptr;

DirAccessUnix::~DirAccessUnix() {
	list_dir_end();
}

#endif // UNIX_ENABLED
