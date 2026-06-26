/**************************************************************************/
/*  memory.hpp                                                            */
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

#ifndef GODOT_MEMORY_HPP
#define GODOT_MEMORY_HPP

#include <cstddef>
#include <cstdint>

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/godot.hpp>

#include <new> // for std::launder
#include <type_traits>
#include <functional> // for std::less

// Detect 32-bit architectures with 128-bit vector units.
#if (defined(__i386__) && (defined(__SSE__) || defined(__SSE2__))) || \
	(defined(_M_IX86) && _M_IX86_FP > 0) || \
	defined(__ARM_NEON)
	#define HAS_128_BIT_SIMD 1
#endif

// Determine the safe minimum stack alignment.
#if !defined(IS_32_BIT) || defined(HAS_128_BIT_SIMD)
	// 64-bit platforms.
	// 32-bit platforms with 128-bit SIMD (x86_32 SSE, etc.).
	#define GODOT_MIN_STACK_ALIGN 16 
#else
	// "Vanilla" 32-bit architectures with no SSE-like goodies.
	#define GODOT_MIN_STACK_ALIGN 8 
#endif

// Calculate the required alignment:
// The larger of the type's requirement or GODOT_MIN_STACK_ALIGN.
#define SAFE_ALIGN_SIZE(m_type) \
	((alignof(m_type) > GODOT_MIN_STACK_ALIGN) ? alignof(m_type) : GODOT_MIN_STACK_ALIGN)

// Unify all safe memory allocation macros
// here for convenience.

#if defined(__GNUC__) || defined(__clang__)
// Use the built-in function for these compilers.
#define SAFE_ALLOCA(m_size, m_align) __builtin_alloca_with_align((m_size), (m_align) * 8)
#elif defined(_WIN32)
// Windows wants `_alloca`
#define SAFE_ALLOCA(m_size, m_align) \
	((void *)((((uintptr_t)_alloca((m_size) + (m_align))) + ((m_align) - 1)) & ~((uintptr_t)((m_align) - 1))))
#else
#define SAFE_ALLOCA(m_size, m_align) \
	((void *)((((uintptr_t)alloca((m_size) + (m_align))) + ((m_align) - 1)) & ~((uintptr_t)((m_align) - 1))))
#endif // __GNUC__ || __clang__

// Safe Stack Allocation Macro. This macro:
// - Allocates requested size + alignment padding.
// - Shifts the pointer to match the type's alignment requirement (alignof).
// - Always guarantees GODOT_MIN_STACK_ALIGN.
// - Safely handles zero-count allocations.
// 
// Should futher prevent crashes on strict RISC architectures
// and improve SIMD safety on x86.
#define SAFE_ALLOCA_ARRAY(m_type, m_count) \
	((m_count) > 0) ? static_cast<m_type *>(SAFE_ALLOCA(sizeof(m_type) * (m_count), SAFE_ALIGN_SIZE(m_type))) : nullptr

// Single-element version.
#define SAFE_ALLOCA_SINGLE(m_type) SAFE_ALLOCA_ARRAY(m_type, 1)

// p_dummy argument is added to avoid conflicts with the engine functions when both engine and GDExtension are built as a static library on iOS.
void *operator new(size_t p_size, const char *p_dummy, const char *p_description); ///< operator new that takes a description and uses MemoryStaticPool
void *operator new(size_t p_size, const char *p_dummy, void *(*p_allocfunc)(size_t p_size)); ///< operator new that takes a description and uses MemoryStaticPool
void *operator new(size_t p_size, const char *p_dummy, void *p_pointer, size_t check, const char *p_description); ///< operator new that takes a description and uses a pointer to the preallocated memory

_ALWAYS_INLINE_ void *operator new(size_t p_size, const char *p_dummy, void *p_pointer, size_t check, const char *p_description) {
	return p_pointer;
}

#ifdef _MSC_VER
// When compiling with VC++ 2017, the above declarations of placement new generate many irrelevant warnings (C4291).
// The purpose of the following definitions is to muffle these warnings, not to provide a usable implementation of placement delete.
void operator delete(void *p_mem, const char *p_dummy, const char *p_description);
void operator delete(void *p_mem, const char *p_dummy, void *(*p_allocfunc)(size_t p_size));
void operator delete(void *p_mem, const char *p_dummy, void *p_pointer, size_t check, const char *p_description);
#endif

namespace godot {

class Wrapped;

class Memory {
	Memory();

public:
	// Force a minimum alignment of 16 bytes.
	// This handles strict-alignment RISC architectures (SPARC, MIPS, Alpha, etc.),
	// ensures SIMD safety, and fixes the MinGW 32-bit compiler bug (GH-113145)
	// by clamping its fluctuating alignof value (8 vs 16) to a consistent 16.
	static constexpr size_t MAX_ALIGN = (alignof(max_align_t) > GODOT_MIN_STACK_ALIGN) ? alignof(max_align_t) : GODOT_MIN_STACK_ALIGN;

	static_assert(MAX_ALIGN % alignof(max_align_t) == 0);

	// Alignment:  ↓ max_align_t        ↓ uint64_t          ↓ MAX_ALIGN
	//             ┌─────────────────┬──┬────────────────┬──┬───────────...
	//             │ uint64_t        │░░│ uint64_t       │░░│ T[]
	//             │ alloc size      │░░│ element count  │░░│ data
	//             └─────────────────┴──┴────────────────┴──┴───────────...
	// Offset:     ↑ SIZE_OFFSET        ↑ ELEMENT_OFFSET    ↑ DATA_OFFSET
	// Note: "alloc size" is used and set by the engine and is never accessed or changed for the extension.

	static constexpr size_t SIZE_OFFSET = 0;
	static constexpr size_t ELEMENT_OFFSET = ((SIZE_OFFSET + sizeof(uint64_t)) % alignof(uint64_t) == 0) ? (SIZE_OFFSET + sizeof(uint64_t)) : ((SIZE_OFFSET + sizeof(uint64_t)) + alignof(uint64_t) - ((SIZE_OFFSET + sizeof(uint64_t)) % alignof(uint64_t)));
	static constexpr size_t DATA_OFFSET = ((ELEMENT_OFFSET + sizeof(uint64_t)) % MAX_ALIGN == 0) ? (ELEMENT_OFFSET + sizeof(uint64_t)) : ((ELEMENT_OFFSET + sizeof(uint64_t)) + MAX_ALIGN - ((ELEMENT_OFFSET + sizeof(uint64_t)) % MAX_ALIGN));

	static void *alloc_static(size_t p_bytes, bool p_pad_align = false);
	static void *realloc_static(void *p_memory, size_t p_bytes, bool p_pad_align = false);
	static void free_static(void *p_ptr, bool p_pad_align = false);
};

template <typename T, std::enable_if_t<!std::is_base_of<::godot::Wrapped, T>::value, bool> = true>
_ALWAYS_INLINE_ void _pre_initialize() {}

_ALWAYS_INLINE_ void postinitialize_handler(void *) {}

template <typename T>
_ALWAYS_INLINE_ T *_post_initialize(T *p_obj) {
	postinitialize_handler(p_obj);
	return p_obj;
}

#define memalloc(m_size) ::godot::Memory::alloc_static(m_size)
#define memrealloc(m_mem, m_size) ::godot::Memory::realloc_static(m_mem, m_size)
#define memfree(m_mem) ::godot::Memory::free_static(m_mem)

#define memnew(m_class) (::godot::_pre_initialize<std::remove_pointer_t<decltype(new ("", "") m_class)>>(), ::godot::_post_initialize(new ("", "") m_class))

#define memnew_allocator(m_class, m_allocator) (::godot::_pre_initialize<std::remove_pointer_t<decltype(new ("", "") m_class)>>(), ::godot::_post_initialize(new ("", m_allocator::alloc) m_class))
#define memnew_placement(m_placement, m_class) (::godot::_pre_initialize<std::remove_pointer_t<decltype(new ("", "") m_class)>>(), ::godot::_post_initialize(new ("", m_placement, sizeof(m_class), "") m_class))

// Generic comparator used in Map, List, etc.
template <typename T>
struct Comparator {
	_ALWAYS_INLINE_ bool operator()(const T &p_a, const T &p_b) const {
		return std::less<T>{}(p_a, p_b);
	}
};

template <typename T>
void memdelete(T *p_class, typename std::enable_if<!std::is_base_of_v<godot::Wrapped, T>>::type * = nullptr) {
	if constexpr (!std::is_trivially_destructible_v<T>) {
		p_class->~T();
	}

	Memory::free_static(p_class);
}

template <typename T, std::enable_if_t<std::is_base_of_v<godot::Wrapped, T>, bool> = true>
void memdelete(T *p_class) {
	godot::internal::gdextension_interface_object_destroy(p_class->_owner);
}

template <typename T, typename A>
void memdelete_allocator(T *p_class) {
	if constexpr (!std::is_trivially_destructible_v<T>) {
		p_class->~T();
	}

	A::free(p_class);
}

class DefaultAllocator {
public:
	_ALWAYS_INLINE_ static void *alloc(size_t p_memory) { return Memory::alloc_static(p_memory); }
	_ALWAYS_INLINE_ static void free(void *p_ptr) { Memory::free_static(p_ptr); }
};

template <typename T>
class DefaultTypedAllocator {
public:
	template <typename... Args>
	_ALWAYS_INLINE_ T *new_allocation(const Args &&...p_args) { return memnew(T(p_args...)); }
	_ALWAYS_INLINE_ void delete_allocation(T *p_allocation) { memdelete(p_allocation); }
};

#define memnew_arr(m_class, m_count) memnew_arr_template<m_class>(m_count)

_FORCE_INLINE_ uint64_t *_get_element_count_ptr(uint8_t *p_ptr) {
	return (uint64_t *)(p_ptr - Memory::DATA_OFFSET + Memory::ELEMENT_OFFSET);
}

template <typename T>
_FORCE_INLINE_ T unaligned_read(const void *p_ptr) {
	if constexpr (std::is_trivially_copyable_v<T>) {
		T local;
		memcpy(&local, p_ptr, sizeof(T));
		return local;
	} else {
#if defined(DEV_ENABLED) || defined(TOOLS_ENABLED)
		const uintptr_t addr = reinterpret_cast<uintptr_t>(p_ptr);
		if (unlikely((addr & (alignof(T) - 1)) != 0)) {
			CRASH_NOW_MSG("FATAL: Unaligned read of non-trivial type.");
		}
#endif
		return *static_cast<const T *>(p_ptr);
	}
}

template <typename T>
_FORCE_INLINE_ void unaligned_write(void *p_ptr, const T &p_val) {
	if constexpr (std::is_trivially_copyable_v<T>) {
		memcpy(p_ptr, &p_val, sizeof(T));
	} else {
#if defined(DEV_ENABLED) || defined(TOOLS_ENABLED)
		const uintptr_t addr = reinterpret_cast<uintptr_t>(p_ptr);
		if (unlikely((addr & (alignof(T) - 1)) != 0)) {
			CRASH_NOW_MSG("FATAL: Unaligned write of non-trivial type.");
		}
#endif
		*static_cast<T *>(p_ptr) = p_val;
	}
}

template <typename ConstructT, typename ArgT>
_FORCE_INLINE_ void unaligned_construct(void *p_ptr, const ArgT &p_arg) {
	const uintptr_t addr = reinterpret_cast<uintptr_t>(p_ptr);
	const bool is_aligned = (addr & (alignof(ConstructT) - 1)) == 0;

	if constexpr (std::is_trivially_copyable_v<ConstructT>) {
		if (is_aligned) {
			::new (p_ptr) ConstructT(p_arg);
		} else {
			ConstructT local(p_arg);
			memcpy(p_ptr, &local, sizeof(ConstructT));
		}
	} else {
#if defined(DEV_ENABLED) || defined(TOOLS_ENABLED)
		if (unlikely(!is_aligned)) {
			CRASH_NOW_MSG("FATAL: Unaligned construction of non-trivial type.");
		}
#endif
		::new (p_ptr) ConstructT(p_arg);
	}
}

// Mutable version
template <typename T>
_FORCE_INLINE_ T *unaligned_ptr_cast(void *p_ptr) {
#if defined(DEV_ENABLED) || defined(TOOLS_ENABLED)
	const uintptr_t addr = reinterpret_cast<uintptr_t>(p_ptr);
	if (unlikely((addr & (alignof(T) - 1)) != 0)) {
		CRASH_NOW_MSG("FATAL: Unaligned pointer cast.");
	}
#endif
	return static_cast<T *>(p_ptr);
}

// For read-only access
template <typename T>
_FORCE_INLINE_ const T *unaligned_ptr_cast(const void *p_ptr) {
#if defined(DEV_ENABLED) || defined(TOOLS_ENABLED)
	const uintptr_t addr = reinterpret_cast<uintptr_t>(p_ptr);
	if (unlikely((addr & (alignof(T) - 1)) != 0)) {
		CRASH_NOW_MSG("FATAL: Unaligned pointer cast.");
	}
#endif
	return static_cast<const T *>(p_ptr);
}

template <typename T>
T *memnew_arr_template(size_t p_elements, const char *p_descr = "") {
	if (p_elements == 0) {
		return nullptr;
	}
	/** overloading operator new[] cannot be done , because it may not return the real allocated address (it may pad the 'element count' before the actual array). Because of that, it must be done by hand. This is the
	same strategy used by std::vector, and the Vector class, so it should be safe.*/

	size_t len = sizeof(T) * p_elements;
	uint8_t *mem = (uint8_t *)Memory::alloc_static(len, true);
	T *failptr = nullptr; // Get rid of a warning.
	ERR_FAIL_NULL_V(mem, failptr);

	uint64_t *_elem_count_ptr = _get_element_count_ptr(mem);
	*(_elem_count_ptr) = p_elements;

	if constexpr (!std::is_trivially_destructible_v<T>) {
		T *elems = (T *)mem;

		/* call operator new */
		for (size_t i = 0; i < p_elements; i++) {
			::new ("", &elems[i], sizeof(T), p_descr) T;
		}
	}

	return (T *)mem;
}

template <typename T>
size_t memarr_len(const T *p_class) {
	uint8_t *ptr = (uint8_t *)p_class;
	uint64_t *_elem_count_ptr = _get_element_count_ptr(ptr);
	return *(_elem_count_ptr);
}

template <typename T>
void memdelete_arr(T *p_class) {
	uint8_t *ptr = (uint8_t *)p_class;

	if constexpr (!std::is_trivially_destructible_v<T>) {
		uint64_t *_elem_count_ptr = _get_element_count_ptr(ptr);
		uint64_t elem_count = *(_elem_count_ptr);

		for (uint64_t i = 0; i < elem_count; i++) {
			p_class[i].~T();
		}
	}

	Memory::free_static(ptr, true);
}

struct _GlobalNil {
	int color = 1;
	_GlobalNil *right;
	_GlobalNil *left;
	_GlobalNil *parent;

	_GlobalNil();
};

struct _GlobalNilClass {
	static _GlobalNil _nil;
};

} // namespace godot

#endif // GODOT_MEMORY_HPP
