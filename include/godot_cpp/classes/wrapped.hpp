/**************************************************************************/
/*  wrapped.hpp                                                           */
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

#ifndef GODOT_WRAPPED_HPP
#define GODOT_WRAPPED_HPP

#include <godot_cpp/core/memory.hpp>

#include <godot_cpp/core/property_info.hpp>

#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/templates/vector.hpp>

#include <godot_cpp/godot.hpp>
#include <type_traits>

#if defined(MACOS_ENABLED) && defined(HOT_RELOAD_ENABLED)
#include <mutex>
#define _GODOT_CPP_AVOID_THREAD_LOCAL
#define _GODOT_CPP_THREAD_LOCAL
#else
#define _GODOT_CPP_THREAD_LOCAL thread_local
#endif

namespace godot {

class ClassDB;

typedef void GodotObject;

template <typename T, std::enable_if_t<std::is_base_of<::godot::Wrapped, T>::value, bool> = true>
_ALWAYS_INLINE_ void _pre_initialize();

class Wrapped;

struct WrappedVTable {
	bool (*setv)(Wrapped *, const StringName &, const Variant &);
	bool (*getv)(const Wrapped *, const StringName &, Variant &);
	void (*get_property_listv)(const Wrapped *, List<PropertyInfo> *);
	void (*validate_propertyv)(const Wrapped *, PropertyInfo &);
	bool (*property_can_revertv)(const Wrapped *, const StringName &);
	bool (*property_get_revertv)(const Wrapped *, const StringName &, Variant &);
	void (*notificationv)(Wrapped *, int, bool);
	String (*to_stringv)(const Wrapped *);
};

template <typename T>
struct ClassMethodDispatcher;

// Base for all engine classes, to contain the pointer to the engine instance.
class Wrapped {
	friend class GDExtensionBinding;
	friend class ClassDB;
	friend void postinitialize_handler(Wrapped *);

	template <typename T, std::enable_if_t<std::is_base_of<::godot::Wrapped, T>::value, bool>>
	friend _ALWAYS_INLINE_ void _pre_initialize();

#ifdef _GODOT_CPP_AVOID_THREAD_LOCAL
	static std::recursive_mutex _constructing_mutex;
#endif

	_GODOT_CPP_THREAD_LOCAL static const StringName *_constructing_extension_class_name;
	_GODOT_CPP_THREAD_LOCAL static const GDExtensionInstanceBindingCallbacks *_constructing_class_binding_callbacks;

#ifdef HOT_RELOAD_ENABLED
	_GODOT_CPP_THREAD_LOCAL static GDExtensionObjectPtr _constructing_recreate_owner;
#endif

	template <typename T>
	_ALWAYS_INLINE_ static void _set_construct_info() {
		_constructing_extension_class_name = T::_get_extension_class_name();
		_constructing_class_binding_callbacks = &T::_gde_binding_callbacks;
	}

protected:
	virtual bool _is_extension_class() const { return false; }
	static const StringName *_get_extension_class_name(); // This is needed to retrieve the class name before the godot object has its _extension and _extension_instance members assigned.

	void _notification(int p_what) {}
	bool _set(const StringName &p_name, const Variant &p_property) { return false; }
	bool _get(const StringName &p_name, Variant &r_property) const { return false; }
	void _get_property_list(List<PropertyInfo> *p_list) const {}
	bool _property_can_revert(const StringName &p_name) const { return false; }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return false; }
	void _validate_property(PropertyInfo &p_property) const {}
	String _to_string() const { return "[" + String(get_class_static()) + ":" + itos(get_instance_id()) + "]"; }

	virtual const WrappedVTable *_get_gd_vtable() const { return nullptr; }

	static bool _call_set_bind(Wrapped *p_this, const StringName &p_name, const Variant &p_property) { return false; }
	static bool _call_get_bind(const Wrapped *p_this, const StringName &p_name, Variant &r_ret) { return false; }
	static void _call_get_property_list_bind(const Wrapped *p_this, List<PropertyInfo> *p_list) {}
	static void _call_validate_property_bind(const Wrapped *p_this, PropertyInfo &p_property) {}
	static bool _call_property_can_revert_bind(const Wrapped *p_this, const StringName &p_name) { return false; }
	static bool _call_property_get_revert_bind(const Wrapped *p_this, const StringName &p_name, Variant &r_ret) { return false; }
	static void _call_notification_bind(Wrapped *p_this, int p_notification, bool p_reversed) {}
	static String _call_to_string_bind(const Wrapped *p_this) { return p_this->_to_string(); }

	// The only reason this has to be held here, is when we return results of `_get_property_list` to Godot, we pass
	// pointers to strings in this list. They have to remain valid to pass the bridge, until the list is freed by Godot...
	::godot::List<::godot::PropertyInfo> plist_owned;

	void _postinitialize();

	Wrapped(const StringName p_godot_class);
	Wrapped(GodotObject *p_godot_object);
	virtual ~Wrapped() {}

public:
	static const StringName &get_class_static() {
		static const StringName string_name = StringName("Wrapped");
		return string_name;
	}

	uint64_t get_instance_id() const {
		return 0;
	}

	// Must be public but you should not touch this.
	GodotObject *_owner = nullptr;
};

template <typename T>
struct ClassMethodDispatcher {
	static bool setv(Wrapped *obj, const StringName &p_name, const Variant &p_property) { return T::_call_set_bind(static_cast<T *>(obj), p_name, p_property); }
	static bool getv(const Wrapped *obj, const StringName &p_name, Variant &r_ret) { return T::_call_get_bind(static_cast<const T *>(obj), p_name, r_ret); }
	static void get_property_listv(const Wrapped *obj, List<PropertyInfo> *p_list) { T::_call_get_property_list_bind(static_cast<const T *>(obj), p_list); }
	static void validate_propertyv(const Wrapped *obj, PropertyInfo &p_property) { T::_call_validate_property_bind(static_cast<const T *>(obj), p_property); }
	static bool property_can_revertv(const Wrapped *obj, const StringName &p_name) { return T::_call_property_can_revert_bind(static_cast<const T *>(obj), p_name); }
	static bool property_get_revertv(const Wrapped *obj, const StringName &p_name, Variant &r_ret) { return T::_call_property_get_revert_bind(static_cast<const T *>(obj), p_name, r_ret); }
	static void notificationv(Wrapped *obj, int p_notification, bool p_reversed) { T::_call_notification_bind(static_cast<T *>(obj), p_notification, p_reversed); }
	static String to_stringv(const Wrapped *obj) { return T::_call_to_string_bind(static_cast<const T *>(obj)); }

	static const WrappedVTable vtable;
};

template <typename T>
const WrappedVTable ClassMethodDispatcher<T>::vtable = {
	&ClassMethodDispatcher<T>::setv,
	&ClassMethodDispatcher<T>::getv,
	&ClassMethodDispatcher<T>::get_property_listv,
	&ClassMethodDispatcher<T>::validate_propertyv,
	&ClassMethodDispatcher<T>::property_can_revertv,
	&ClassMethodDispatcher<T>::property_get_revertv,
	&ClassMethodDispatcher<T>::notificationv,
	&ClassMethodDispatcher<T>::to_stringv
};

template <typename T, std::enable_if_t<std::is_base_of<::godot::Wrapped, T>::value, bool>>
_ALWAYS_INLINE_ void _pre_initialize() {
#ifdef _GODOT_CPP_AVOID_THREAD_LOCAL
	Wrapped::_constructing_mutex.lock();
#endif
	Wrapped::_set_construct_info<T>();
}

_FORCE_INLINE_ void snarray_add_str(Vector<StringName> &arr) {
}

_FORCE_INLINE_ void snarray_add_str(Vector<StringName> &arr, const StringName &p_str) {
	arr.push_back(p_str);
}

template <typename... P>
_FORCE_INLINE_ void snarray_add_str(Vector<StringName> &arr, const StringName &p_str, P... p_args) {
	arr.push_back(p_str);
	snarray_add_str(arr, p_args...);
}

template <typename... P>
_FORCE_INLINE_ Vector<StringName> snarray(P... p_args) {
	Vector<StringName> arr;
	snarray_add_str(arr, p_args...);
	return arr;
}

namespace internal {

GDExtensionPropertyInfo *create_c_property_list(const ::godot::List<::godot::PropertyInfo> &plist_cpp, uint32_t *r_size);
void free_c_property_list(GDExtensionPropertyInfo *plist);

typedef void (*EngineClassRegistrationCallback)();
void add_engine_class_registration_callback(EngineClassRegistrationCallback p_callback);
void register_engine_class(const StringName &p_name, const GDExtensionInstanceBindingCallbacks *p_callbacks);
void register_engine_classes();

template <typename T>
struct EngineClassRegistration {
	EngineClassRegistration() {
		add_engine_class_registration_callback(&EngineClassRegistration<T>::callback);
	}

	static void callback() {
		register_engine_class(T::get_class_static(), &T::_gde_binding_callbacks);
	}
};

} // namespace internal

} // namespace godot

// Use this on top of your own classes.
// Note: the trail of `***` is to keep sane diffs in PRs, because clang-format otherwise moves every `\` which makes
// every line of the macro different
#define GDCLASS(m_class, m_inherits) /***********************************************************************************************************************************************/ \
private:                                                                                                                                                                               \
	void operator=(const m_class & /*p_rval*/) {}                                                                                                                                      \
	friend class ::godot::ClassDB;                                                                                                                                                     \
	friend class ::godot::Wrapped;                                                                                                                                                     \
																																													   \
protected:                                                                                                                                                                             \
	virtual bool _is_extension_class() const override { return true; }                                                                                                                 \
	virtual const ::godot::WrappedVTable *_get_gd_vtable() const override { return &::godot::ClassMethodDispatcher<m_class>::vtable; }                                                 \
																																													   \
	static const ::godot::StringName *_get_extension_class_name() {                                                                                                                    \
		const ::godot::StringName &string_name = get_class_static();                                                                                                                   \
		return &string_name;                                                                                                                                                           \
	}                                                                                                                                                                                  \
																																													   \
	static void (*_get_bind_methods())() {                                                                                                                                             \
		return &m_class::_bind_methods;                                                                                                                                                \
	}                                                                                                                                                                                  \
																																													   \
public:                                                                                                                                                                                \
	static bool _call_set_bind(m_class *p_this, const ::godot::StringName &p_name, const ::godot::Variant &p_property) {                                                               \
		if constexpr (std::is_same_v<decltype(&m_class::_set), bool (m_class::*)(const ::godot::StringName &, const ::godot::Variant &)>) {                                            \
			if (p_this->_set(p_name, p_property)) {                                                                                                                                    \
				return true;                                                                                                                                                           \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return m_inherits::_call_set_bind(p_this, p_name, p_property);                                                                                                                 \
	}                                                                                                                                                                                  \
																																													   \
	static bool _call_get_bind(const m_class *p_this, const ::godot::StringName &p_name, ::godot::Variant &r_ret) {                                                                    \
		if constexpr (std::is_same_v<decltype(&m_class::_get), bool (m_class::*)(const ::godot::StringName &, ::godot::Variant &) const>) {                                            \
			if (p_this->_get(p_name, r_ret)) {                                                                                                                                         \
				return true;                                                                                                                                                           \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return m_inherits::_call_get_bind(p_this, p_name, r_ret);                                                                                                                      \
	}                                                                                                                                                                                  \
																																													   \
	static void _call_get_property_list_bind(const m_class *p_this, ::godot::List<::godot::PropertyInfo> *p_list) {                                                                    \
		m_inherits::_call_get_property_list_bind(p_this, p_list);                                                                                                                      \
		if constexpr (std::is_same_v<decltype(&m_class::_get_property_list), void (m_class::*)(::godot::List<::godot::PropertyInfo> *) const>) {                                       \
			p_this->_get_property_list(p_list);                                                                                                                                        \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static bool _call_property_can_revert_bind(const m_class *p_this, const ::godot::StringName &p_name) {                                                                             \
		if constexpr (std::is_same_v<decltype(&m_class::_property_can_revert), bool (m_class::*)(const ::godot::StringName &) const>) {                                                \
			if (p_this->_property_can_revert(p_name)) {                                                                                                                                \
				return true;                                                                                                                                                           \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return m_inherits::_call_property_can_revert_bind(p_this, p_name);                                                                                                             \
	}                                                                                                                                                                                  \
																																													   \
	static bool _call_property_get_revert_bind(const m_class *p_this, const ::godot::StringName &p_name, ::godot::Variant &r_ret) {                                                    \
		if constexpr (std::is_same_v<decltype(&m_class::_property_get_revert), bool (m_class::*)(const ::godot::StringName &, ::godot::Variant &) const>) {                            \
			if (p_this->_property_get_revert(p_name, r_ret)) {                                                                                                                         \
				return true;                                                                                                                                                           \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return m_inherits::_call_property_get_revert_bind(p_this, p_name, r_ret);                                                                                                      \
	}                                                                                                                                                                                  \
																																													   \
	static void _call_validate_property_bind(const m_class *p_this, ::godot::PropertyInfo &p_property) {                                                                               \
		m_inherits::_call_validate_property_bind(p_this, p_property);                                                                                                                  \
		if constexpr (std::is_same_v<decltype(&m_class::_validate_property), void (m_class::*)(::godot::PropertyInfo &) const>) {                                                      \
			p_this->_validate_property(p_property);                                                                                                                                    \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static void _call_notification_bind(m_class *p_this, int p_notification, bool p_reversed) {                                                                                        \
		if (!p_reversed) {                                                                                                                                                             \
			m_inherits::_call_notification_bind(p_this, p_notification, p_reversed);                                                                                                   \
		}                                                                                                                                                                              \
		if constexpr (std::is_same_v<decltype(&m_class::_notification), void (m_class::*)(int)>) {                                                                                     \
			p_this->_notification(p_notification);                                                                                                                                     \
		}                                                                                                                                                                              \
		if (p_reversed) {                                                                                                                                                              \
			m_inherits::_call_notification_bind(p_this, p_notification, p_reversed);                                                                                                   \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static ::godot::String _call_to_string_bind(const m_class *p_this) {                                                                                                               \
		if constexpr (std::is_same_v<decltype(&m_class::_to_string), ::godot::String (m_class::*)() const>) {                                                                          \
			return p_this->_to_string();                                                                                                                                               \
		}                                                                                                                                                                              \
		return m_inherits::_call_to_string_bind(p_this);                                                                                                                               \
	}                                                                                                                                                                                  \
																																													   \
	template <typename T, typename B>                                                                                                                                                  \
	static void register_virtuals() {                                                                                                                                                  \
		m_inherits::template register_virtuals<T, B>();                                                                                                                                \
	}                                                                                                                                                                                  \
																																													   \
	typedef m_class self_type;                                                                                                                                                         \
	typedef m_inherits parent_type;                                                                                                                                                    \
																																													   \
	static void initialize_class() {                                                                                                                                                   \
		static bool initialized = false;                                                                                                                                               \
		if (initialized) {                                                                                                                                                             \
			return;                                                                                                                                                                    \
		}                                                                                                                                                                              \
		m_inherits::initialize_class();                                                                                                                                                \
		if (m_class::_get_bind_methods() != m_inherits::_get_bind_methods()) {                                                                                                         \
			_bind_methods();                                                                                                                                                           \
			m_inherits::template register_virtuals<m_class, m_inherits>();                                                                                                             \
		}                                                                                                                                                                              \
		initialized = true;                                                                                                                                                            \
	}                                                                                                                                                                                  \
																																													   \
	static const ::godot::StringName &get_class_static() {                                                                                                                             \
		static const ::godot::StringName string_name = ::godot::StringName(U## #m_class);                                                                                              \
		return string_name;                                                                                                                                                            \
	}                                                                                                                                                                                  \
																																													   \
	static const ::godot::StringName &get_parent_class_static() {                                                                                                                      \
		return m_inherits::get_class_static();                                                                                                                                         \
	}                                                                                                                                                                                  \
																																													   \
	static void notification_bind(GDExtensionClassInstancePtr p_instance, int32_t p_what, GDExtensionBool p_reversed) {                                                                \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				vt->notificationv(cls, p_what, p_reversed);                                                                                                                            \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static GDExtensionBool set_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value) {                                \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				return vt->setv(cls, *reinterpret_cast<const ::godot::StringName *>(p_name), *reinterpret_cast<const ::godot::Variant *>(p_value));                                    \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return false;                                                                                                                                                                  \
	}                                                                                                                                                                                  \
																																													   \
	static GDExtensionBool get_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) {                                       \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				return vt->getv(cls, *reinterpret_cast<const ::godot::StringName *>(p_name), *reinterpret_cast<::godot::Variant *>(r_ret));                                            \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return false;                                                                                                                                                                  \
	}                                                                                                                                                                                  \
																																													   \
	static inline bool has_get_property_list() {                                                                                                                                       \
		if constexpr (std::is_same_v<decltype(&m_class::_get_property_list), void (m_class::*)(::godot::List<::godot::PropertyInfo> *) const>) {                                       \
			return true;                                                                                                                                                               \
		} else {                                                                                                                                                                       \
			return m_inherits::has_get_property_list();                                                                                                                                \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static const GDExtensionPropertyInfo *get_property_list_bind(GDExtensionClassInstancePtr p_instance, uint32_t *r_count) {                                                          \
		if (!p_instance) {                                                                                                                                                             \
			if (r_count)                                                                                                                                                               \
				*r_count = 0;                                                                                                                                                          \
			return nullptr;                                                                                                                                                            \
		}                                                                                                                                                                              \
		m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                        \
		::godot::List<::godot::PropertyInfo> &plist_cpp = cls->plist_owned;                                                                                                            \
		ERR_FAIL_COND_V_MSG(!plist_cpp.is_empty(), nullptr, "Internal error, property list was not freed by engine!");                                                                 \
		if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                                \
			vt->get_property_listv(cls, &plist_cpp);                                                                                                                                   \
		}                                                                                                                                                                              \
		return ::godot::internal::create_c_property_list(plist_cpp, r_count);                                                                                                          \
	}                                                                                                                                                                                  \
																																													   \
	static void free_property_list_bind(GDExtensionClassInstancePtr p_instance, const GDExtensionPropertyInfo *p_list, uint32_t /*p_count*/) {                                         \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			cls->plist_owned.clear();                                                                                                                                                  \
			::godot::internal::free_c_property_list(const_cast<GDExtensionPropertyInfo *>(p_list));                                                                                    \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static GDExtensionBool property_can_revert_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name) {                                                    \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				return vt->property_can_revertv(cls, *reinterpret_cast<const ::godot::StringName *>(p_name));                                                                          \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return false;                                                                                                                                                                  \
	}                                                                                                                                                                                  \
																																													   \
	static GDExtensionBool property_get_revert_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) {                       \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				return vt->property_get_revertv(cls, *reinterpret_cast<const ::godot::StringName *>(p_name), *reinterpret_cast<::godot::Variant *>(r_ret));                            \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return false;                                                                                                                                                                  \
	}                                                                                                                                                                                  \
																																													   \
	static GDExtensionBool validate_property_bind(GDExtensionClassInstancePtr p_instance, GDExtensionPropertyInfo *p_property) {                                                       \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				::godot::PropertyInfo info(p_property);                                                                                                                                \
				vt->validate_propertyv(cls, info);                                                                                                                                     \
				info._update(p_property);                                                                                                                                              \
				return true;                                                                                                                                                           \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
		return false;                                                                                                                                                                  \
	}                                                                                                                                                                                  \
																																													   \
	static void to_string_bind(GDExtensionClassInstancePtr p_instance, GDExtensionBool *r_is_valid, GDExtensionStringPtr r_out) {                                                      \
		if (p_instance) {                                                                                                                                                              \
			m_class *cls = reinterpret_cast<m_class *>(p_instance);                                                                                                                    \
			if (const ::godot::WrappedVTable *vt = cls->_get_gd_vtable()) {                                                                                                            \
				*reinterpret_cast<::godot::String *>(r_out) = vt->to_stringv(cls);                                                                                                     \
				*r_is_valid = true;                                                                                                                                                    \
			}                                                                                                                                                                          \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static void free(void * /*data*/, GDExtensionClassInstancePtr ptr) {                                                                                                               \
		if (ptr) {                                                                                                                                                                     \
			m_class *cls = reinterpret_cast<m_class *>(ptr);                                                                                                                           \
			cls->~m_class();                                                                                                                                                           \
			::godot::Memory::free_static(cls);                                                                                                                                         \
		}                                                                                                                                                                              \
	}                                                                                                                                                                                  \
																																													   \
	static void *_gde_binding_create_callback(void * /*p_token*/, void * /*p_instance*/) {                                                                                             \
		return nullptr;                                                                                                                                                                \
	}                                                                                                                                                                                  \
																																													   \
	static void _gde_binding_free_callback(void * /*p_token*/, void * /*p_instance*/, void * /*p_binding*/) {                                                                          \
	}                                                                                                                                                                                  \
																																													   \
	static GDExtensionBool _gde_binding_reference_callback(void * /*p_token*/, void * /*p_instance*/, GDExtensionBool /*p_reference*/) {                                               \
		return true;                                                                                                                                                                   \
	}                                                                                                                                                                                  \
																																													   \
	static constexpr GDExtensionInstanceBindingCallbacks _gde_binding_callbacks = {                                                                                                    \
		_gde_binding_create_callback,                                                                                                                                                  \
		_gde_binding_free_callback,                                                                                                                                                    \
		_gde_binding_reference_callback,                                                                                                                                               \
	};                                                                                                                                                                                 \
																																													   \
private:

// Don't use this for your classes, use GDCLASS() instead.
#define GDEXTENSION_CLASS_ALIAS(m_class, m_alias_for, m_inherits) /******************************************************************************************************************/ \
private:                                                                                                                                                                               \
	inline static ::godot::internal::EngineClassRegistration<m_class> _gde_engine_class_registration_helper;                                                                           \
	void operator=(const m_class &p_rval) {}                                                                                                                                           \
	friend class ::godot::ClassDB;                                                                                                                                                     \
	friend class ::godot::Wrapped;                                                                                                                                                     \
																																													   \
protected:                                                                                                                                                                             \
	m_class(const char *p_godot_class) : m_inherits(p_godot_class) {}                                                                                                                  \
	m_class(GodotObject *p_godot_object) : m_inherits(p_godot_object) {}                                                                                                               \
																																													   \
	virtual const ::godot::WrappedVTable *_get_gd_vtable() const override { return nullptr; }                                                                                          \
	static void _bind_methods() {}                                                                                                                                                     \
	static void (*_get_bind_methods())() { return nullptr; }                                                                                                                           \
																																													   \
	static bool _call_set_bind(m_class *p_this, const ::godot::StringName &p_name, const ::godot::Variant &p_property) { return false; }                                               \
	static bool _call_get_bind(const m_class *p_this, const ::godot::StringName &p_name, ::godot::Variant &r_ret) { return false; }                                                    \
	static void _call_get_property_list_bind(const m_class *p_this, ::godot::List<::godot::PropertyInfo> *p_list) {}                                                                   \
	static void _call_validate_property_bind(const m_class *p_this, ::godot::PropertyInfo &p_property) {}                                                                              \
	static bool _call_property_can_revert_bind(const m_class *p_this, const ::godot::StringName &p_name) { return false; }                                                             \
	static bool _call_property_get_revert_bind(const m_class *p_this, const ::godot::StringName &p_name, ::godot::Variant &r_ret) { return false; }                                    \
	static void _call_notification_bind(m_class *p_this, int p_notification, bool p_reversed) {}                                                                                       \
	static ::godot::String _call_to_string_bind(const m_class *p_this) { return ""; }                                                                                                  \
																																													   \
public:                                                                                                                                                                                \
	typedef m_class self_type;                                                                                                                                                         \
	typedef m_inherits parent_type;                                                                                                                                                    \
																																													   \
	static void initialize_class() {}                                                                                                                                                  \
																																													   \
	static const ::godot::StringName &get_class_static() {                                                                                                                             \
		static const ::godot::StringName string_name = ::godot::StringName(#m_alias_for);                                                                                              \
		return string_name;                                                                                                                                                            \
	}                                                                                                                                                                                  \
																																													   \
	static void notification_bind(GDExtensionClassInstancePtr p_instance, int32_t p_what, GDExtensionBool p_reversed) {}                                                               \
	static GDExtensionBool set_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value) { return false; }                \
	static GDExtensionBool get_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) { return false; }                       \
	static GDExtensionBool property_can_revert_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name) { return false; }                                    \
	static GDExtensionBool property_get_revert_bind(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) { return false; }       \
	static GDExtensionBool validate_property_bind(GDExtensionClassInstancePtr p_instance, GDExtensionPropertyInfo *p_property) { return false; }                                       \
	static void to_string_bind(GDExtensionClassInstancePtr p_instance, GDExtensionBool *r_is_valid, GDExtensionStringPtr r_out) {}                                                     \
	static const ::godot::StringName &get_parent_class_static() { return m_inherits::get_class_static(); }                                                                             \
																																													   \
	static inline bool has_get_property_list() { return false; }                                                                                                                       \
																																													   \
	static void free(void *data, GDExtensionClassInstancePtr ptr) {}                                                                                                                   \
																																													   \
	static void *_gde_binding_create_callback(void *p_token, void *p_instance) {                                                                                                       \
		/* Do not call memnew here, we don't want the post-initializer to be called */                                                                                                 \
		return new ("", "") m_class((GodotObject *)p_instance);                                                                                                                        \
	}                                                                                                                                                                                  \
	static void _gde_binding_free_callback(void *p_token, void *p_instance, void *p_binding) {                                                                                         \
		/* Explicitly call the deconstructor to ensure proper lifecycle for non-trivial members */                                                                                     \
		reinterpret_cast<m_class *>(p_binding)->~m_class();                                                                                                                            \
		Memory::free_static(reinterpret_cast<m_class *>(p_binding));                                                                                                                   \
	}                                                                                                                                                                                  \
	static GDExtensionBool _gde_binding_reference_callback(void *p_token, void *p_instance, GDExtensionBool p_reference) { return true; }                                              \
																																													   \
	static constexpr GDExtensionInstanceBindingCallbacks _gde_binding_callbacks = {                                                                                                    \
		_gde_binding_create_callback,                                                                                                                                                  \
		_gde_binding_free_callback,                                                                                                                                                    \
		_gde_binding_reference_callback,                                                                                                                                               \
	};                                                                                                                                                                                 \
	m_class() : m_class(#m_alias_for) {}                                                                                                                                               \
																																													   \
private:

// Don't use this for your classes, use GDCLASS() instead.
#define GDEXTENSION_CLASS(m_class, m_inherits) GDEXTENSION_CLASS_ALIAS(m_class, m_class, m_inherits)

#define GDVIRTUAL_CALL(m_name, ...) _gdvirtual_##m_name##_call(__VA_ARGS__)
#define GDVIRTUAL_CALL_PTR(m_obj, m_name, ...) m_obj->_gdvirtual_##m_name##_call(__VA_ARGS__)

#define GDVIRTUAL_BIND(m_name, ...) ::godot::ClassDB::add_virtual_method(get_class_static(), _gdvirtual_##m_name##_get_method_info(), ::godot::snarray(__VA_ARGS__));
#define GDVIRTUAL_IS_OVERRIDDEN(m_name) _gdvirtual_##m_name##_overridden()
#define GDVIRTUAL_IS_OVERRIDDEN_PTR(m_obj, m_name) m_obj->_gdvirtual_##m_name##_overridden()

#endif // GODOT_WRAPPED_HPP
