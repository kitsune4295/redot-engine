/**************************************************************************/
/*  gltf_buffer_view.compat.inc                                           */
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

#ifndef DISABLE_DEPRECATED

GLTFBufferIndex GLTFBufferView::_get_buffer_bind_compat_86907() {
	return get_buffer();
}

int GLTFBufferView::_get_byte_offset_bind_compat_86907() {
	return get_byte_offset();
}

int GLTFBufferView::_get_byte_length_bind_compat_86907() {
	return get_byte_length();
}

int GLTFBufferView::_get_byte_stride_bind_compat_86907() {
	return get_byte_stride();
}

bool GLTFBufferView::_get_indices_bind_compat_86907() {
	return get_indices();
}

void GLTFBufferView::_bind_compatibility_methods() {
	// Non-const versions for compatibility.
	ClassDB::bind_compatibility_method(D_METHOD("get_buffer"), &GLTFBufferView::_get_buffer_bind_compat_86907);
	ClassDB::bind_compatibility_method(D_METHOD("get_byte_offset"), &GLTFBufferView::_get_byte_offset_bind_compat_86907);
	ClassDB::bind_compatibility_method(D_METHOD("get_byte_length"), &GLTFBufferView::_get_byte_length_bind_compat_86907);
	ClassDB::bind_compatibility_method(D_METHOD("get_byte_stride"), &GLTFBufferView::_get_byte_stride_bind_compat_86907);
	ClassDB::bind_compatibility_method(D_METHOD("get_indices"), &GLTFBufferView::_get_indices_bind_compat_86907);
}

#endif // DISABLE_DEPRECATED
