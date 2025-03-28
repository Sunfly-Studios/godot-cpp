/**************************************************************************/
/*  method_ptrcall.hpp                                                    */
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

#ifndef GODOT_METHOD_PTRCALL_HPP
#define GODOT_METHOD_PTRCALL_HPP

#include <godot_cpp/core/defs.hpp>

#include <godot_cpp/core/object.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

template <typename T>
struct PtrToArg {};

#define MAKE_PTRARG(m_type)                                                \
    template <>                                                            \
    struct PtrToArg<m_type> {                                              \
        _FORCE_INLINE_ static m_type convert(const void *p_ptr) {          \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];          \
            memcpy(buf, p_ptr, sizeof(m_type));                            \
            return *reinterpret_cast<const m_type *>(buf);                 \
        }                                                                  \
        typedef m_type EncodeT;                                            \
        _FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {     \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];          \
            memcpy(buf, p_ptr, sizeof(m_type));                            \
            m_type *dst = reinterpret_cast<m_type *>(buf);                 \
            *dst = p_val;                                                  \
            memcpy(p_ptr, buf, sizeof(m_type));                            \
        }                                                                  \
    };                                                                     \
    template <>                                                            \
    struct PtrToArg<const m_type &> {                                      \
        _FORCE_INLINE_ static m_type convert(const void *p_ptr) {          \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];          \
            memcpy(buf, p_ptr, sizeof(m_type));                            \
            return *reinterpret_cast<const m_type *>(buf);                 \
        }                                                                  \
        typedef m_type EncodeT;                                            \
        _FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {     \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];          \
            memcpy(buf, p_ptr, sizeof(m_type));                            \
            m_type *dst = reinterpret_cast<m_type *>(buf);                 \
            *dst = p_val;                                                  \
            memcpy(p_ptr, buf, sizeof(m_type));                            \
        }                                                                  \
    }

#define MAKE_PTRARGCONV(m_type, m_conv)                                  \
    template <>                                                          \
    struct PtrToArg<m_type> {                                            \
        _FORCE_INLINE_ static m_type convert(const void *p_ptr) {        \
            m_conv c;                                                    \
            memcpy(&c, p_ptr, sizeof(m_conv));                           \
            return static_cast<m_type>(c);                               \
        }                                                                \
        typedef m_conv EncodeT;                                          \
        _FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {   \
            m_conv c = static_cast<m_conv>(p_val);                       \
            memcpy(p_ptr, &c, sizeof(m_conv));                           \
        }                                                                \
        _FORCE_INLINE_ static m_conv encode_arg(m_type p_val) {          \
            return static_cast<m_conv>(p_val);                           \
        }                                                                \
    };                                                                   \
    template <>                                                          \
    struct PtrToArg<const m_type &> {                                    \
        _FORCE_INLINE_ static m_type convert(const void *p_ptr) {        \
            m_conv c;                                                    \
            memcpy(&c, p_ptr, sizeof(m_conv));                           \
            return static_cast<m_type>(c);                               \
        }                                                                \
        typedef m_conv EncodeT;                                          \
        _FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {   \
            m_conv c = static_cast<m_conv>(p_val);                       \
            memcpy(p_ptr, &c, sizeof(m_conv));                           \
        }                                                                \
        _FORCE_INLINE_ static m_conv encode_arg(m_type p_val) {          \
            return static_cast<m_conv>(p_val);                           \
        }                                                                \
    }

#define MAKE_PTRARG_BY_REFERENCE(m_type)                                       \
    template <>                                                                \
    struct PtrToArg<m_type> {                                                  \
        _FORCE_INLINE_ static m_type convert(const void *p_ptr) {              \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];              \
            memcpy(buf, p_ptr, sizeof(m_type));                                \
            return *reinterpret_cast<const m_type *>(buf);                     \
        }                                                                      \
        typedef m_type EncodeT;                                                \
        _FORCE_INLINE_ static void encode(const m_type &p_val, void *p_ptr) {  \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];              \
            memcpy(buf, p_ptr, sizeof(m_type));                                \
            m_type *dst = reinterpret_cast<m_type *>(buf);                     \
            *dst = p_val;                                                      \
            memcpy(p_ptr, buf, sizeof(m_type));                                \
        }                                                                      \
    };                                                                         \
    template <>                                                                \
    struct PtrToArg<const m_type &> {                                          \
        _FORCE_INLINE_ static m_type convert(const void *p_ptr) {              \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];              \
            memcpy(buf, p_ptr, sizeof(m_type));                                \
            return *reinterpret_cast<const m_type *>(buf);                     \
        }                                                                      \
        typedef m_type EncodeT;                                                \
        _FORCE_INLINE_ static void encode(const m_type &p_val, void *p_ptr) {  \
            alignas(alignof(m_type)) uint8_t buf[sizeof(m_type)];              \
            memcpy(buf, p_ptr, sizeof(m_type));                                \
            m_type *dst = reinterpret_cast<m_type *>(buf);                     \
            *dst = p_val;                                                      \
            memcpy(p_ptr, buf, sizeof(m_type));                                \
        }                                                                      \
    }

MAKE_PTRARGCONV(bool, uint8_t);
// Integer types.
MAKE_PTRARGCONV(uint8_t, int64_t);
MAKE_PTRARGCONV(int8_t, int64_t);
MAKE_PTRARGCONV(uint16_t, int64_t);
MAKE_PTRARGCONV(int16_t, int64_t);
MAKE_PTRARGCONV(uint32_t, int64_t);
MAKE_PTRARGCONV(int32_t, int64_t);
MAKE_PTRARGCONV(char16_t, int64_t);
MAKE_PTRARGCONV(char32_t, int64_t);
MAKE_PTRARGCONV(wchar_t, int64_t);
MAKE_PTRARG(int64_t);
MAKE_PTRARG(uint64_t);
// Float types
MAKE_PTRARGCONV(float, double);
MAKE_PTRARG(double);

MAKE_PTRARG(String);
MAKE_PTRARG(Vector2);
MAKE_PTRARG(Vector2i);
MAKE_PTRARG(Rect2);
MAKE_PTRARG(Rect2i);
MAKE_PTRARG_BY_REFERENCE(Vector3);
MAKE_PTRARG_BY_REFERENCE(Vector3i);
MAKE_PTRARG(Transform2D);
MAKE_PTRARG_BY_REFERENCE(Vector4);
MAKE_PTRARG_BY_REFERENCE(Vector4i);
MAKE_PTRARG_BY_REFERENCE(Plane);
MAKE_PTRARG(Quaternion);
MAKE_PTRARG_BY_REFERENCE(AABB);
MAKE_PTRARG_BY_REFERENCE(Basis);
MAKE_PTRARG_BY_REFERENCE(Transform3D);
MAKE_PTRARG_BY_REFERENCE(Projection);
MAKE_PTRARG_BY_REFERENCE(Color);
MAKE_PTRARG(StringName);
MAKE_PTRARG(NodePath);
MAKE_PTRARG(RID);
// Object doesn't need this.
MAKE_PTRARG(Callable);
MAKE_PTRARG(Signal);
MAKE_PTRARG(Dictionary);
MAKE_PTRARG(Array);
MAKE_PTRARG(PackedByteArray);
MAKE_PTRARG(PackedInt32Array);
MAKE_PTRARG(PackedInt64Array);
MAKE_PTRARG(PackedFloat32Array);
MAKE_PTRARG(PackedFloat64Array);
MAKE_PTRARG(PackedStringArray);
MAKE_PTRARG(PackedVector2Array);
MAKE_PTRARG(PackedVector3Array);
MAKE_PTRARG(PackedVector4Array);
MAKE_PTRARG(PackedColorArray);
MAKE_PTRARG_BY_REFERENCE(Variant);

// This is for Object.

template <typename T>
struct PtrToArg<T *> {
    static_assert(std::is_base_of<Object, T>::value, "Cannot encode non-Object value as an Object");
    _FORCE_INLINE_ static T *convert(const void *p_ptr) {
        if (unlikely(!p_ptr)) {
            return nullptr;
        }
        // GDExtensionObjectPtr is a pointer type (opaque handle)
        GDExtensionObjectPtr obj_handle;
        memcpy(&obj_handle, p_ptr, sizeof(GDExtensionObjectPtr));
        
        return reinterpret_cast<T *>(godot::internal::get_object_instance_binding(obj_handle));
    }
    typedef Object *EncodeT;
    _FORCE_INLINE_ static void encode(T *p_var, void *p_ptr) {
        // Retrieve the opaque handle (void*) from the object
        GDExtensionObjectPtr obj_handle = likely(p_var) ? p_var->_owner : nullptr;
        // Copy the handle into the buffer safely
        memcpy(p_ptr, &obj_handle, sizeof(GDExtensionObjectPtr));
    }
};

template <typename T>
struct PtrToArg<const T *> {
    static_assert(std::is_base_of<Object, T>::value, "Cannot encode non-Object value as an Object");
    _FORCE_INLINE_ static const T *convert(const void *p_ptr) {
        if (unlikely(!p_ptr)) {
            return nullptr;
        }
        GDExtensionObjectPtr obj_handle;
        memcpy(&obj_handle, p_ptr, sizeof(GDExtensionObjectPtr));
        
        return reinterpret_cast<const T *>(godot::internal::get_object_instance_binding(obj_handle));
    }
    typedef const Object *EncodeT;
    _FORCE_INLINE_ static void encode(T *p_var, void *p_ptr) {
        GDExtensionObjectPtr obj_handle = likely(p_var) ? p_var->_owner : nullptr;
        memcpy(p_ptr, &obj_handle, sizeof(GDExtensionObjectPtr));
    }
};

// Pointers.
#define GDVIRTUAL_NATIVE_PTR(m_type)                                          \
    template <>                                                               \
    struct PtrToArg<m_type *> {                                               \
        _FORCE_INLINE_ static m_type *convert(const void *p_ptr) {            \
            void *ptr;                                                        \
            memcpy(&ptr, p_ptr, sizeof(void *));                              \
            return (m_type *)ptr;                                             \
        }                                                                     \
        typedef m_type *EncodeT;                                              \
        _FORCE_INLINE_ static void encode(m_type *p_var, void *p_ptr) {       \
            void *ptr = (void *)p_var;                                        \
            memcpy(p_ptr, &ptr, sizeof(void *));                              \
        }                                                                     \
    };                                                                        \
    template <>                                                               \
    struct PtrToArg<const m_type *> {                                         \
        _FORCE_INLINE_ static const m_type *convert(const void *p_ptr) {      \
            void *ptr;                                                        \
            memcpy(&ptr, p_ptr, sizeof(void *));                              \
            return (const m_type *)ptr;                                       \
        }                                                                     \
        typedef const m_type *EncodeT;                                        \
        _FORCE_INLINE_ static void encode(const m_type *p_var, void *p_ptr) { \
            void *ptr = (void *)p_var;                                        \
            memcpy(p_ptr, &ptr, sizeof(void *));                              \
        }                                                                     \
    }

GDVIRTUAL_NATIVE_PTR(void);
GDVIRTUAL_NATIVE_PTR(bool);
GDVIRTUAL_NATIVE_PTR(char);
GDVIRTUAL_NATIVE_PTR(char16_t);
GDVIRTUAL_NATIVE_PTR(char32_t);
GDVIRTUAL_NATIVE_PTR(wchar_t);
GDVIRTUAL_NATIVE_PTR(uint8_t);
GDVIRTUAL_NATIVE_PTR(uint8_t *);
GDVIRTUAL_NATIVE_PTR(int8_t);
GDVIRTUAL_NATIVE_PTR(uint16_t);
GDVIRTUAL_NATIVE_PTR(int16_t);
GDVIRTUAL_NATIVE_PTR(uint32_t);
GDVIRTUAL_NATIVE_PTR(int32_t);
GDVIRTUAL_NATIVE_PTR(int64_t);
GDVIRTUAL_NATIVE_PTR(uint64_t);
GDVIRTUAL_NATIVE_PTR(float);
GDVIRTUAL_NATIVE_PTR(double);

} // namespace godot

#endif // GODOT_METHOD_PTRCALL_HPP
