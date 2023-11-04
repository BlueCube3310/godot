/**************************************************************************/
/*  file_access_memory.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
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

#include "file_access_memory.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/templates/rb_map.h"

static HashMap<String, Vector<uint8_t>> *files = nullptr;

void FileAccessMemory::register_file(String p_name, Vector<uint8_t> p_data) {
	if (!files) {
		files = memnew((HashMap<String, Vector<uint8_t>>));
	}

	String name;
	if (ProjectSettings::get_singleton()) {
		name = ProjectSettings::get_singleton()->globalize_path(p_name);
	} else {
		name = p_name;
	}
	//name = DirAccess::normalize_path(name);

	(*files)[name] = p_data;
}

void FileAccessMemory::cleanup() {
	if (!files) {
		return;
	}

	memdelete(files);
}

Ref<FileAccess> FileAccessMemory::create() {
	return memnew(FileAccessMemory);
}

bool FileAccessMemory::file_exists(const String &p_name) {
	String name = fix_path(p_name);
	//name = DirAccess::normalize_path(name);

	return files && (files->find(name) != nullptr);
}

Error FileAccessMemory::open_custom(const uint8_t *p_data, uint64_t p_len) {
	data = (uint8_t *)p_data;
	length = p_len;
	pos = 0;
	return OK;
}

Error FileAccessMemory::open_internal(const String &p_path, int p_mode_flags) {
	ERR_FAIL_NULL_V(files, ERR_FILE_NOT_FOUND);

	String name = fix_path(p_path);
	//name = DirAccess::normalize_path(name);

	HashMap<String, Vector<uint8_t>>::Iterator E = files->find(name);
	ERR_FAIL_COND_V_MSG(!E, ERR_FILE_NOT_FOUND, "Can't find file '" + p_path + "'.");

	data = E->value.ptrw();
	length = E->value.size();
	pos = 0;

	return OK;
}

bool FileAccessMemory::is_open() const {
	return data != nullptr;
}

void FileAccessMemory::seek(uint64_t p_position) {
	ERR_FAIL_NULL(data);
	pos = p_position;
}

void FileAccessMemory::seek_end(int64_t p_position) {
	ERR_FAIL_NULL(data);
	pos = length + p_position;
}

uint64_t FileAccessMemory::get_position() const {
	ERR_FAIL_NULL_V(data, 0);
	return pos;
}

uint64_t FileAccessMemory::get_length() const {
	ERR_FAIL_NULL_V(data, 0);
	return length;
}

bool FileAccessMemory::eof_reached() const {
	return pos > length;
}

uint8_t FileAccessMemory::get_8() const {
	uint8_t ret = 0;
	if (pos < length) {
		ret = data[pos];
	}
	++pos;

	return ret;
}

uint16_t FileAccessMemory::get_16() const {
	uint16_t ret = 0;
	get_buffer(reinterpret_cast<uint8_t *>(&ret), 2);

	if (big_endian) {
		ret = BSWAP16(ret);
	}

	return ret;
}

uint32_t FileAccessMemory::get_32() const {
	uint32_t ret = 0;
	get_buffer(reinterpret_cast<uint8_t *>(&ret), 4);

	if (big_endian) {
		ret = BSWAP32(ret);
	}

	return ret;
}

uint64_t FileAccessMemory::get_64() const {
	uint64_t ret = 0;
	get_buffer(reinterpret_cast<uint8_t *>(&ret), 8);

	if (big_endian) {
		ret = BSWAP64(ret);
	}

	return ret;
}

uint64_t FileAccessMemory::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);
	ERR_FAIL_NULL_V(data, -1);

	uint64_t left = length - pos;
	uint64_t read = MIN(p_length, left);

	if (read < p_length) {
		WARN_PRINT("Reading less data than requested");
	}

	memcpy(p_dst, &data[pos], read);
	pos += read;

	return read;
}

Error FileAccessMemory::get_error() const {
	return pos >= length ? ERR_FILE_EOF : OK;
}

void FileAccessMemory::flush() {
	ERR_FAIL_NULL(data);
}

void FileAccessMemory::store_8(uint8_t p_byte) {
	ERR_FAIL_NULL(data);
	ERR_FAIL_COND(pos >= length);
	data[pos++] = p_byte;
}

void FileAccessMemory::store_16(uint16_t p_bytes) {
	ERR_FAIL_NULL(data);
	ERR_FAIL_COND(pos >= length);

	if (big_endian) {
		p_bytes = BSWAP16(p_bytes);
	}

	store_buffer(reinterpret_cast<uint8_t *>(&p_bytes), 2);
}

void FileAccessMemory::store_32(uint32_t p_bytes) {
	ERR_FAIL_NULL(data);
	ERR_FAIL_COND(pos + 3 >= length);

	if (big_endian) {
		p_bytes = BSWAP32(p_bytes);
	}

	store_buffer(reinterpret_cast<uint8_t *>(&p_bytes), 4);
}

void FileAccessMemory::store_64(uint64_t p_bytes) {
	ERR_FAIL_NULL(data);
	ERR_FAIL_COND(pos >= length);

	if (big_endian) {
		p_bytes = BSWAP64(p_bytes);
	}

	store_buffer(reinterpret_cast<uint8_t *>(&p_bytes), 8);
}

void FileAccessMemory::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND(!p_src && p_length > 0);
	uint64_t left = length - pos;
	uint64_t write = MIN(p_length, left);
	if (write < p_length) {
		WARN_PRINT("Writing less data than requested");
	}

	memcpy(&data[pos], p_src, write);
	pos += write;
}
