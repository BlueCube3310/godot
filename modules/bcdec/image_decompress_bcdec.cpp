/**************************************************************************/
/*  image_decompress_bcdec.cpp                                            */
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

#include "image_decompress_bcdec.h"

#define BCDEC_IMPLEMENTATION
#include "thirdparty/misc/bcdec.h"

// Helper functions to ensure the image gets decompressed to 4 channels.
void bcdec_bc4_rgba(const void *compressedBlock, void *decompressedBlock, int destinationPitch) {
	bcdec__smooth_alpha_block(compressedBlock, decompressedBlock, destinationPitch, 4);
}

void bcdec_bc5_rgba(const void *compressedBlock, void *decompressedBlock, int destinationPitch) {
	bcdec__smooth_alpha_block(compressedBlock, decompressedBlock, destinationPitch, 4);
	bcdec__smooth_alpha_block(((char *)compressedBlock) + 8, ((char *)decompressedBlock) + 1, destinationPitch, 4);
}

static void decompress_image(BCdecFormat format, const void *src, void *dst, uint64_t width, uint64_t height) {
	const uint8_t *src_blocks = reinterpret_cast<const uint8_t *>(src);
	uint32_t *dec_blocks = reinterpret_cast<uint32_t *>(dst);
	uint64_t src_pos = 0;

#define DECOMPRESS_LOOP(func, block_size)                                      \
	for (uint64_t y = 0; y < height; y += 4) {                                 \
		for (uint64_t x = 0; x < width; x += 4) {                              \
			func(&src_blocks[src_pos], &dec_blocks[y * width + x], width * 4); \
			src_pos += block_size;                                             \
		}                                                                      \
	}

	switch (format) {
		case BCdec_BC1: {
			DECOMPRESS_LOOP(bcdec_bc1, BCDEC_BC1_BLOCK_SIZE)
		} break;
		case BCdec_BC2: {
			DECOMPRESS_LOOP(bcdec_bc2, BCDEC_BC2_BLOCK_SIZE)
		} break;
		case BCdec_BC3: {
			DECOMPRESS_LOOP(bcdec_bc3, BCDEC_BC3_BLOCK_SIZE)
		} break;
		case BCdec_BC4: {
			DECOMPRESS_LOOP(bcdec_bc4_rgba, BCDEC_BC4_BLOCK_SIZE)
		} break;
		case BCdec_BC5: {
			DECOMPRESS_LOOP(bcdec_bc5_rgba, BCDEC_BC5_BLOCK_SIZE)
		} break;
	}

#undef DECOMPRESS_LOOP
}

void image_decompress_bcdec(Image *p_image) {
	int w = p_image->get_width();
	int h = p_image->get_height();

	Image::Format source_format = p_image->get_format();
	Image::Format target_format = Image::FORMAT_RGBA8;

	Vector<uint8_t> data;
	int target_size = Image::get_image_data_size(w, h, target_format, p_image->has_mipmaps());
	int mm_count = p_image->get_mipmap_count();
	data.resize(target_size);

	const uint8_t *rb = p_image->get_data().ptr();
	uint8_t *wb = data.ptrw();

	BCdecFormat dec_format = BCdec_BC1;

	switch (source_format) {
		case Image::FORMAT_DXT1:
			dec_format = BCdec_BC1;
			break;

		case Image::FORMAT_DXT3:
			dec_format = BCdec_BC2;
			break;

		case Image::FORMAT_DXT5:
		case Image::FORMAT_DXT5_RA_AS_RG:
			dec_format = BCdec_BC3;
			break;

		case Image::FORMAT_RGTC_R:
			dec_format = BCdec_BC4;
			break;

		case Image::FORMAT_RGTC_RG:
			dec_format = BCdec_BC5;
			break;

		default:
			ERR_FAIL_MSG("Bcdec: Can't decompress unknown format: " + itos(p_image->get_format()) + ".");
			break;
	}

	if (source_format == Image::FORMAT_RGTC_R || source_format == Image::FORMAT_RGTC_RG) {
		// Set the entire texture's color to opaque black.
		uint32_t *wb32 = reinterpret_cast<uint32_t *>(wb);
		for (int i = 0; i < data.size() / 4; i++) {
			wb32[i] = 0xFF000000;
		}
	}

	// Decompress mipmaps.
	for (int i = 0; i <= mm_count; i++) {
		int src_ofs = 0, mipmap_size = 0, mipmap_w = 0, mipmap_h = 0;
		p_image->get_mipmap_offset_size_and_dimensions(i, src_ofs, mipmap_size, mipmap_w, mipmap_h);

		int dst_ofs = Image::get_image_mipmap_offset(p_image->get_width(), p_image->get_height(), target_format, i);
		decompress_image(dec_format, rb + src_ofs, wb + dst_ofs, mipmap_w, mipmap_h);

		w >>= 1;
		h >>= 1;
	}

	p_image->set_data(p_image->get_width(), p_image->get_height(), p_image->has_mipmaps(), target_format, data);

	// Swap channels if necessary.
	if (source_format == Image::FORMAT_DXT5_RA_AS_RG) {
		p_image->convert_ra_rgba8_to_rg();
	}
}
