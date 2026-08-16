/**************************************************************************/
/*  rid_owner.hpp                                                         */
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

#ifndef GODOT_RID_OWNER_HPP
#define GODOT_RID_OWNER_HPP

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/templates/spin_lock.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdio>
#include <typeinfo>
#include <atomic>

namespace godot {

template <typename T, bool THREAD_SAFE = false>
class RID_Alloc {
	struct Chunk {
		alignas(T) uint8_t data[sizeof(T)];
		uint32_t validator;
	};
	Chunk **chunks = nullptr;
	uint32_t **free_list_chunks = nullptr;

	uint32_t elements_in_chunk = 0;
	uint32_t max_alloc = 0;
	uint32_t alloc_count = 0;
	uint32_t chunk_limit = 0;

	const char *description = nullptr;

	mutable SpinLock spin_lock;

	void _ensure_initialized() {
		if (likely(elements_in_chunk > 0)) {
			// Already initialised
			return;
		}

		const uint32_t target_chunk_byte_size = 65536;
		const uint32_t maximum_number_of_elements = 262144;

		if (sizeof(T) > target_chunk_byte_size) {
			elements_in_chunk = 1;
		} else {
			elements_in_chunk = (target_chunk_byte_size / sizeof(T));
		}

		// Handle thread-safe pre-allocations if they haven't happened yet
		if constexpr (THREAD_SAFE) {
			if (chunks == nullptr) {
				chunk_limit = (maximum_number_of_elements / elements_in_chunk) + 1;
				chunks = static_cast<Chunk **>(Memory::alloc_aligned_static(sizeof(Chunk *) * chunk_limit, alignof(Chunk *)));
				free_list_chunks = static_cast<uint32_t **>(Memory::alloc_aligned_static(sizeof(uint32_t *) * chunk_limit, alignof(uint32_t *)));
				
				// Clear the pre-allocated array pointers
				for (uint32_t i = 0; i < chunk_limit; i++) {
					chunks[i] = nullptr;
					free_list_chunks[i] = nullptr;
				}
				std::atomic_signal_fence(std::memory_order_release);
			}
		}
	}

	_FORCE_INLINE_ RID _allocate_rid() {
		if constexpr (THREAD_SAFE) {
			spin_lock.lock();
		}

		_ensure_initialized();

		if (alloc_count == max_alloc) {
			//allocate a new chunk
			uint32_t chunk_count = alloc_count == 0 ? 0 : (max_alloc / elements_in_chunk);
			if (THREAD_SAFE && chunk_count == chunk_limit) {
				spin_lock.unlock();
				ERR_FAIL_V_MSG(RID(), "Element limit reached.");
			}

			//grow chunks
			if constexpr (!THREAD_SAFE) {
				chunks = static_cast<Chunk **>(Memory::realloc_aligned_static(chunks, sizeof(Chunk *) * (chunk_count + 1), sizeof(Chunk *) * chunk_count, alignof(Chunk *)));
			}
			chunks[chunk_count] = static_cast<Chunk *>(Memory::alloc_aligned_static(sizeof(Chunk) * elements_in_chunk, alignof(Chunk))); //but don't initialize
			//grow free lists
			if constexpr (!THREAD_SAFE) {
				free_list_chunks = static_cast<uint32_t **>(Memory::realloc_aligned_static(free_list_chunks, sizeof(uint32_t *) * (chunk_count + 1), sizeof(uint32_t *) * chunk_count, alignof(uint32_t *)));
			}
			free_list_chunks[chunk_count] = static_cast<uint32_t *>(Memory::alloc_aligned_static(sizeof(uint32_t) * elements_in_chunk, alignof(uint32_t)));

			//initialize
			for (uint32_t i = 0; i < elements_in_chunk; i++) {
				// Formally begin lifetime
				Chunk *c = ::new (static_cast<void *>(&chunks[chunk_count][i])) Chunk;
				c->validator = 0xFFFFFFFF;
				free_list_chunks[chunk_count][i] = alloc_count + i;
			}

			if constexpr (THREAD_SAFE) {
				// Store atomically to avoid data race with the load in get_or_null().
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#if defined(__has_warning)
	#if __has_warning("-Watomic-alignment")
		#pragma GCC diagnostic ignored "-Watomic-alignment"
	#endif
#endif
				__atomic_store_n(&max_alloc, max_alloc + elements_in_chunk, __ATOMIC_RELAXED);
#pragma GCC diagnostic pop
#else
				((std::atomic<uint32_t> *)&max_alloc)->store(max_alloc + elements_in_chunk, std::memory_order_relaxed);
#endif
			} else {
				max_alloc += elements_in_chunk;
			}
		}

		uint32_t free_index = free_list_chunks[alloc_count / elements_in_chunk][alloc_count % elements_in_chunk];

		uint32_t free_chunk = free_index / elements_in_chunk;
		uint32_t free_element = free_index % elements_in_chunk;

		uint32_t validator = (uint32_t)(UtilityFunctions::rid_allocate_id() & 0x7FFFFFFF);
		CRASH_COND_MSG(validator == 0x7FFFFFFF, "Overflow in RID validator");
		uint64_t id = validator;
		id <<= 32;
		id |= free_index;

		chunks[free_chunk][free_element].validator = validator;
		chunks[free_chunk][free_element].validator |= 0x80000000; //mark uninitialized bit

		alloc_count++;

		if constexpr (THREAD_SAFE) {
			spin_lock.unlock();
		}

		return UtilityFunctions::rid_from_int64(id);
	}

public:
	RID make_rid() {
		RID rid = _allocate_rid();
		initialize_rid(rid);
		return rid;
	}
	RID make_rid(const T &p_value) {
		RID rid = _allocate_rid();
		initialize_rid(rid, p_value);
		return rid;
	}

	// allocate but don't initialize, use initialize_rid afterwards
	RID allocate_rid() {
		return _allocate_rid();
	}

	_FORCE_INLINE_ T *get_or_null(const RID &p_rid, bool p_initialize = false) {
		if (p_rid == RID()) {
			return nullptr;
		}
		
		if constexpr (THREAD_SAFE) {
			std::atomic_signal_fence(std::memory_order_acquire);
		}

		uint64_t id = p_rid.get_id();
		uint32_t idx = uint32_t(id & 0xFFFFFFFF);
		uint32_t ma;
		
		if constexpr (THREAD_SAFE) { // Read atomically to avoid data race with the store in _allocate_rid().
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#if defined(__has_warning)
	#if __has_warning("-Watomic-alignment")
		#pragma GCC diagnostic ignored "-Watomic-alignment"
	#endif
#endif
			ma = __atomic_load_n(&max_alloc, __ATOMIC_RELAXED);
#pragma GCC diagnostic pop
#else
			ma = ((std::atomic<uint32_t> *)&max_alloc)->load(std::memory_order_relaxed);
#endif
		} else {
			ma = max_alloc;
		}
		
		if (unlikely(idx >= ma)) {
			return nullptr;
		}

		uint32_t idx_chunk = idx / elements_in_chunk;
		uint32_t idx_element = idx % elements_in_chunk;

		uint32_t validator = uint32_t(id >> 32);
		
		Chunk &c = chunks[idx_chunk][idx_element];

		if (unlikely(p_initialize)) {
			if (unlikely(!(c.validator & 0x80000000))) {
				ERR_FAIL_V_MSG(nullptr, "Initializing already initialized RID");
			}

			if (unlikely((c.validator & 0x7FFFFFFF) != validator)) {
				ERR_FAIL_V_MSG(nullptr, "Attempting to initialize the wrong RID");
			}

			c.validator &= 0x7FFFFFFF; // initialized

			// Return raw pointer for placement new.
			return reinterpret_cast<T *>(&c.data);

		} else if (unlikely(c.validator != validator)) {
			if ((c.validator & 0x80000000) && c.validator != 0xFFFFFFFF) {
				ERR_FAIL_V_MSG(nullptr, "Attempting to use an uninitialized RID");
			}
			return nullptr;
		}

		// Object is alive.
		return std::launder(reinterpret_cast<T *>(&c.data));
	}
	
	void initialize_rid(RID p_rid) {
		T *mem = get_or_null(p_rid, true);
		ERR_FAIL_NULL(mem);
		unaligned_construct<T>(mem);
		if constexpr (THREAD_SAFE) {
			std::atomic_signal_fence(std::memory_order_release);
		}
	}
	
	void initialize_rid(RID p_rid, const T &p_value) {
		T *mem = get_or_null(p_rid, true);
		ERR_FAIL_NULL(mem);
		unaligned_construct<T>(mem, p_value);
		if constexpr (THREAD_SAFE) {
			std::atomic_signal_fence(std::memory_order_release);
		}
	}

	_FORCE_INLINE_ bool owns(const RID &p_rid) const {
		if constexpr (THREAD_SAFE) {
			spin_lock.lock();
		}

		uint64_t id = p_rid.get_id();
		uint32_t idx = uint32_t(id & 0xFFFFFFFF);
		if (unlikely(idx >= max_alloc)) {
			if constexpr (THREAD_SAFE) {
				spin_lock.unlock();
			}
			return false;
		}

		uint32_t idx_chunk = idx / elements_in_chunk;
		uint32_t idx_element = idx % elements_in_chunk;

		uint32_t validator = uint32_t(id >> 32);

		bool owned = (validator != 0x7FFFFFFF) && (chunks[idx_chunk][idx_element].validator & 0x7FFFFFFF) == validator;

		if constexpr (THREAD_SAFE) {
			spin_lock.unlock();
		}

		return owned;
	}

	_FORCE_INLINE_ void free(const RID &p_rid) {
		if constexpr (THREAD_SAFE) {
			spin_lock.lock();
		}

		uint64_t id = p_rid.get_id();
		uint32_t idx = uint32_t(id & 0xFFFFFFFF);
		if (unlikely(idx >= max_alloc)) {
			if constexpr (THREAD_SAFE) {
				spin_lock.unlock();
			}
			ERR_FAIL();
		}

		uint32_t idx_chunk = idx / elements_in_chunk;
		uint32_t idx_element = idx % elements_in_chunk;

		uint32_t validator = uint32_t(id >> 32);
		if (unlikely(chunks[idx_chunk][idx_element].validator & 0x80000000)) {
			if constexpr (THREAD_SAFE) {
				spin_lock.unlock();
			}
			ERR_FAIL_MSG("Attempted to free an uninitialized or invalid RID");
		} else if (unlikely(chunks[idx_chunk][idx_element].validator != validator)) {
			if constexpr (THREAD_SAFE) {
				spin_lock.unlock();
			}
			ERR_FAIL();
		}

		unaligned_destroy<T>(&chunks[idx_chunk][idx_element].data);
		chunks[idx_chunk][idx_element].validator = 0xFFFFFFFF; // go invalid

		alloc_count--;
		free_list_chunks[alloc_count / elements_in_chunk][alloc_count % elements_in_chunk] = idx;

		if constexpr (THREAD_SAFE) {
			spin_lock.unlock();
		}
	}

	_FORCE_INLINE_ uint32_t get_rid_count() const {
		return alloc_count;
	}

	void get_owned_list(List<RID> *p_owned) const {
		if constexpr (THREAD_SAFE) {
			spin_lock.lock();
		}
		for (size_t i = 0; i < max_alloc; i++) {
			uint64_t validator = chunks[i / elements_in_chunk][i % elements_in_chunk].validator;
			if (validator != 0xFFFFFFFF) {
				p_owned->push_back(UtilityFunctions::rid_from_int64((validator << 32) | i));
			}
		}
		if constexpr (THREAD_SAFE) {
			spin_lock.unlock();
		}
	}

	// used for fast iteration in the elements or RIDs
	void fill_owned_buffer(RID *p_rid_buffer) const {
		if constexpr (THREAD_SAFE) {
			spin_lock.lock();
		}
		uint32_t idx = 0;
		for (size_t i = 0; i < max_alloc; i++) {
			uint64_t validator = chunks[i / elements_in_chunk][i % elements_in_chunk].validator;
			if (validator != 0xFFFFFFFF) {
				p_rid_buffer[idx] = UtilityFunctions::rid_from_int64((validator << 32) | i);
				idx++;
			}
		}
		if constexpr (THREAD_SAFE) {
			spin_lock.unlock();
		}
	}

	void set_description(const char *p_descrption) {
		description = p_descrption;
	}

	RID_Alloc(uint32_t p_target_chunk_byte_size = 65536, uint32_t p_maximum_number_of_elements = 262144) {
		if (elements_in_chunk > 0) {
			return; // Already initialised
		}
		
		if (sizeof(T) > p_target_chunk_byte_size) {
			elements_in_chunk = 1;
		} else {
			elements_in_chunk = (p_target_chunk_byte_size / sizeof(T));
		}
		if constexpr (THREAD_SAFE) {
			chunk_limit = (p_maximum_number_of_elements / elements_in_chunk) + 1;
			chunks = static_cast<Chunk **>(Memory::alloc_aligned_static(sizeof(Chunk *) * chunk_limit, alignof(Chunk *)));
			free_list_chunks = static_cast<uint32_t **>(Memory::alloc_aligned_static(sizeof(uint32_t *) * chunk_limit, alignof(uint32_t *)));
			std::atomic_signal_fence(std::memory_order_release);
		}
	}

	~RID_Alloc() {
		if constexpr (THREAD_SAFE) {
			std::atomic_signal_fence(std::memory_order_acquire);
		}

		if (alloc_count) {
			if (description) {
				printf("ERROR: %d  RID allocations of type '%s' were leaked at exit.", alloc_count, description);
			} else {
#ifdef NO_SAFE_CAST
				printf("ERROR: %d RID allocations of type 'unknown' were leaked at exit.", alloc_count);
#else
				printf("ERROR: %d RID allocations of type '%s' were leaked at exit.", alloc_count, typeid(T).name());
#endif
			}

			if constexpr (!std::is_trivially_destructible_v<T>) {
				for (size_t i = 0; i < max_alloc; i++) {
					uint64_t validator = chunks[i / elements_in_chunk][i % elements_in_chunk].validator;
					if (validator & 0x80000000) {
						continue; // uninitialized
					}
					if (validator != 0xFFFFFFFF) {
						unaligned_destroy<T>(&chunks[i / elements_in_chunk][i % elements_in_chunk].data);
					}
				}
			}
		}

		uint32_t chunk_count = max_alloc / elements_in_chunk;
		for (uint32_t i = 0; i < chunk_count; i++) {
			Memory::free_aligned_static(chunks[i]);
			Memory::free_aligned_static(free_list_chunks[i]);
		}

		if (chunks) {
			Memory::free_aligned_static(chunks);
			Memory::free_aligned_static(free_list_chunks);
		}
	}
};

template <typename T, bool THREAD_SAFE = false>
class RID_PtrOwner {
	RID_Alloc<T *, THREAD_SAFE> alloc;

public:
	_FORCE_INLINE_ RID make_rid(T *p_ptr) {
		return alloc.make_rid(p_ptr);
	}

	_FORCE_INLINE_ RID allocate_rid() {
		return alloc.allocate_rid();
	}

	_FORCE_INLINE_ void initialize_rid(RID p_rid, T *p_ptr) {
		alloc.initialize_rid(p_rid, p_ptr);
	}

	_FORCE_INLINE_ T *get_or_null(const RID &p_rid) {
		T **ptr = alloc.get_or_null(p_rid);
		if (unlikely(!ptr)) {
			return nullptr;
		}
		return *ptr;
	}

	_FORCE_INLINE_ void replace(const RID &p_rid, T *p_new_ptr) {
		T **ptr = alloc.get_or_null(p_rid);
		ERR_FAIL_NULL(ptr);
		*ptr = p_new_ptr;
	}

	_FORCE_INLINE_ bool owns(const RID &p_rid) const {
		return alloc.owns(p_rid);
	}

	_FORCE_INLINE_ void free(const RID &p_rid) {
		alloc.free(p_rid);
	}

	_FORCE_INLINE_ uint32_t get_rid_count() const {
		return alloc.get_rid_count();
	}

	_FORCE_INLINE_ void get_owned_list(List<RID> *p_owned) const {
		return alloc.get_owned_list(p_owned);
	}

	void fill_owned_buffer(RID *p_rid_buffer) const {
		alloc.fill_owned_buffer(p_rid_buffer);
	}

	void set_description(const char *p_descrption) {
		alloc.set_description(p_descrption);
	}

	RID_PtrOwner(uint32_t p_target_chunk_byte_size = 65536, uint32_t p_maximum_number_of_elements = 262144) :
			alloc(p_target_chunk_byte_size, p_maximum_number_of_elements) {}
};

template <typename T, bool THREAD_SAFE = false>
class RID_Owner {
	RID_Alloc<T, THREAD_SAFE> alloc;

public:
	_FORCE_INLINE_ RID make_rid() {
		return alloc.make_rid();
	}
	_FORCE_INLINE_ RID make_rid(const T &p_ptr) {
		return alloc.make_rid(p_ptr);
	}

	_FORCE_INLINE_ RID allocate_rid() {
		return alloc.allocate_rid();
	}

	_FORCE_INLINE_ void initialize_rid(RID p_rid) {
		alloc.initialize_rid(p_rid);
	}

	_FORCE_INLINE_ void initialize_rid(RID p_rid, const T &p_ptr) {
		alloc.initialize_rid(p_rid, p_ptr);
	}

	_FORCE_INLINE_ T *get_or_null(const RID &p_rid) {
		return alloc.get_or_null(p_rid);
	}

	_FORCE_INLINE_ bool owns(const RID &p_rid) const {
		return alloc.owns(p_rid);
	}

	_FORCE_INLINE_ void free(const RID &p_rid) {
		alloc.free(p_rid);
	}

	_FORCE_INLINE_ uint32_t get_rid_count() const {
		return alloc.get_rid_count();
	}

	_FORCE_INLINE_ void get_owned_list(List<RID> *p_owned) const {
		return alloc.get_owned_list(p_owned);
	}
	void fill_owned_buffer(RID *p_rid_buffer) const {
		alloc.fill_owned_buffer(p_rid_buffer);
	}

	void set_description(const char *p_descrption) {
		alloc.set_description(p_descrption);
	}
	
	RID_Owner(uint32_t p_target_chunk_byte_size = 65536, uint32_t p_maximum_number_of_elements = 262144) :
			alloc(p_target_chunk_byte_size, p_maximum_number_of_elements) {}
};

} // namespace godot

#endif // GODOT_RID_OWNER_HPP
