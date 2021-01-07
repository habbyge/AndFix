/*
 *
 * Copyright (c) 2011 The Android Open Source Project
 * Copyright (c) 2015, alipay.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <stdint.h>    /* C99 */

namespace art {
namespace mirror {
class Object {
public:
  // The number of vtable entries in java.lang.Object.
  static constexpr size_t kVTableLength = 11;
  static uint32_t hash_code_seed;
  uint32_t klass_;

  uint32_t monitor_;
};

class Class: public Object {
public:
  // A magic value for reference_instance_offsets_. Ignore the bits and walk the super chain when
  // this is the value.
  // [This is an unlikely "natural" value, since it would be 30 non-ref instance fields followed by
  // 2 ref instance fields.]
  static constexpr uint32_t kClassWalkSuper = 0xC0000000;
  // Interface method table size. Increasing this value reduces the chance of two interface methods
  // colliding in the interface method table but increases the size of classes that implement
  // (non-marker) interfaces.
  static constexpr size_t kImtSize = 0;	//IMT_SIZE;
  // Defining class loader, or null for the "bootstrap" system loader.
  uint32_t class_loader_;
  // For array classes, the component class object for instanceof/checkcast
  // (for String[][][], this will be String[][]). null for non-array classes.
  uint32_t component_type_;
  // DexCache of resolved constant pool entries (will be null for classes generated by the
  // runtime such as arrays and primitive classes).
  uint32_t dex_cache_;
  // Short cuts to dex_cache_ member for fast compiled code access.
  uint32_t dex_cache_strings_;
  // The interface table (iftable_) contains pairs of a interface class and an array of the
  // interface methods. There is one pair per interface supported by this class.  That means one
  // pair for each interface we support directly, indirectly via superclass, or indirectly via a
  // superinterface.  This will be null if neither we nor our superclass implement any interfaces.
  //
  // Why we need this: given "class Foo implements Face", declare "Face faceObj = new Foo()".
  // Invoke faceObj.blah(), where "blah" is part of the Face interface.  We can't easily use a
  // single vtable.
  //
  // For every interface a concrete class implements, we create an array of the concrete vtable_
  // methods for the methods in the interface.
  uint32_t iftable_;
  // Descriptor for the class such as "java.lang.Class" or "[C". Lazily initialized by ComputeName
  uint32_t name_;
  // The superclass, or null if this is java.lang.Object or a primitive type.
  //
  // Note that interfaces have java.lang.Object as their
  // superclass. This doesn't match the expectations in JNI
  // GetSuperClass or java.lang.Class.getSuperClass() which need to
  // check for interfaces and return null.
  uint32_t super_class_;
  // If class verify fails, we must return same error on subsequent tries.
  uint32_t verify_error_class_;
  // Virtual method table (vtable), for use by "invoke-virtual".  The vtable from the superclass is
  // copied in, and virtual methods from our class either replace those from the super or are
  // appended. For abstract classes, methods may be created in the vtable that aren't in
  // virtual_ methods_ for miranda methods.
  uint32_t vtable_;
  // Access flags; low 16 bits are defined by VM spec.
  // Note: Shuffled back.
  uint32_t access_flags_;
  // static, private, and <init> methods. Pointer to an ArtMethod length-prefixed array.
  uint64_t direct_methods_;
  // instance fields
  //
  // These describe the layout of the contents of an Object.
  // Note that only the fields directly declared by this class are
  // listed in ifields; fields declared by a superclass are listed in
  // the superclass's Class.ifields.
  //
  // ArtFields are allocated as a length prefixed ArtField array, and not an array of pointers to
  // ArtFields.
  uint64_t ifields_;
  // Static fields length-prefixed array.
  uint64_t sfields_;
  // Virtual methods defined in this class; invoked through vtable. Pointer to an ArtMethod
  // length-prefixed array.
  uint64_t virtual_methods_;
  // Total size of the Class instance; used when allocating storage on gc heap.
  // See also object_size_.
  uint32_t class_size_;
  // Tid used to check for recursive <clinit> invocation.
  pid_t clinit_thread_id_;
  // ClassDef index in dex file, -1 if no class definition such as an array.
  // TODO: really 16bits
  int32_t dex_class_def_idx_;
  // Type index in dex file.
  // TODO: really 16bits
  int32_t dex_type_idx_;
  // Number of direct fields.
  uint32_t num_direct_methods_;
  // Number of instance fields.
  uint32_t num_instance_fields_;
  // Number of instance fields that are object refs.
  uint32_t num_reference_instance_fields_;
  // Number of static fields that are object refs,
  uint32_t num_reference_static_fields_;
  // Number of static fields.
  uint32_t num_static_fields_;
  // Number of virtual methods.
  uint32_t num_virtual_methods_;
  // Total object size; used when allocating storage on gc heap.
  // (For interfaces and abstract classes this will be zero.)
  // See also class_size_.
  uint32_t object_size_;
  // The lower 16 bits contains a Primitive::Type value. The upper 16
  // bits contains the size shift of the primitive type.
  uint32_t primitive_type_;
  // Bitmap of offsets of ifields.
  uint32_t reference_instance_offsets_;
  // State of class initialization.
  uint32_t status_;
  // TODO: ?
  // initiating class loader list
  // NOTE: for classes with low serialNumber, these are unused, and the
  // values are kept in a table in gDvm.
  // InitiatingLoaderList initiating_loader_list_;
  // The following data exist in real class objects.
  // Embedded Imtable, for class object that's not an interface, fixed size.
  // ImTableEntry embedded_imtable_[0];
  // Embedded Vtable, for class object that's not an interface, variable size.
  // VTableEntry embedded_vtable_[0];
  // Static fields, variable size.
  // uint32_t fields_[0];
  // java.lang.Class
  static uint32_t java_lang_Class_;
};

class ArtField {
public:
  uint32_t declaring_class_;
  uint32_t access_flags_;
  uint32_t field_dex_idx_;
  uint32_t offset_;
};

class ArtMethod {
public:
    
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  // The class we are a part of.
  uint32_t declaring_class_;
  // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
  uint32_t dex_cache_resolved_methods_;
  // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
  uint32_t dex_cache_resolved_types_;
  // Access flags; low 16 bits are defined by spec.
  uint32_t access_flags_;
  /* Dex file fields. The defining dex file is available via declaring_class_->dex_cache_ */
  // Offset to the CodeItem.
  uint32_t dex_code_item_offset_;
  // Index into method_ids of the dex file associated with this method.
  uint32_t dex_method_index_;
  /* End of dex file fields. */
  // Entry within a dispatch table for this method. For static/direct methods the index is into
  // the declaringClass.directMethods, for virtual methods the vtable and for interface methods the
  // ifTable.
  uint32_t method_index_;
  // Fake padding field gets inserted here.
  // Must be the last fields in the method.
  // PACKED(4) is necessary for the correctness of
  // RoundUp(OFFSETOF_MEMBER(ArtMethod, ptr_sized_fields_), pointer_size).
  struct PtrSizedFields {
    // Method dispatch from the interpreter invokes this pointer which may cause a bridge into
    // compiled code.
    void* entry_point_from_interpreter_;
    // Pointer to JNI function registered to this method, or a function to resolve the JNI function.
    void* entry_point_from_jni_;
    // Method dispatch from quick compiled code invokes this pointer which may cause bridging into
    // the interpreter.
    void* entry_point_from_quick_compiled_code_;
  } ptr_sized_fields_;
};

}

}
