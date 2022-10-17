// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//===-- wasmedge/common/enum_types.h - WASM types related enumerations ----===//
//
// Part of the WasmEdge Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definitions of WASM types related enumerations.
///
//===----------------------------------------------------------------------===//

#ifndef WASMEDGE_C_API_ENUM_TYPES_H
#define WASMEDGE_C_API_ENUM_TYPES_H

/// WASM Value type C enumeration.
enum WasmEdge_ValType {
#define UseValType
#define Line(NAME, VALUE, STRING) WasmEdge_ValType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseValType
};

/// WASM Number type C enumeration.
enum WasmEdge_NumType {
#define UseNumType
#define Line(NAME, VALUE) WasmEdge_NumType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseNumType
};

/// WASM Reference type C enumeration.
enum WasmEdge_RefType {
#define UseRefType
#define Line(NAME, VALUE) WasmEdge_RefType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseRefType
};

/// WASM Mutability C enumeration.
enum WasmEdge_Mutability {
#define UseValMut
#define Line(NAME, VALUE, STRING) WasmEdge_Mutability_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseValMut
};

/// WASM External type C enumeration.
enum WasmEdge_ExternalType {
#define UseExternalType
#define Line(NAME, VALUE, STRING) WasmEdge_ExternalType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseExternalType
};

/// WASM GC proposal storage type type C enumeration.
enum WasmEdge_GcStorageType {
#define UseGcStorageType
#define Line(NAME, VALUE, STRING) WasmEdge_GcStorageType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseGcStorageType
};

/// WASM GC proposal reference type type C enumeration.
enum WasmEdge_GcReferenceType {
#define UseGcReferenceType
#define Line(NAME, VALUE, STRING) WasmEdge_GcReferenceType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseGcReferenceType
};

/// WASM GC proposal builtin heap type type C enumeration.
enum WasmEdge_GcBuiltinHeapType {
#define UseGcBuiltinHeapType
#define Line(NAME, VALUE, STRING) WasmEdge_GcBuiltinHeapType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseGcBuiltinHeapType
};

/// WASM GC proposal structure type type C enumeration.
enum WasmEdge_GcStructureType {
#define UseGcStructureType
#define Line(NAME, VALUE, STRING) WasmEdge_GcStructureType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseGcStructureType
};

/// WASM GC proposal sub type type C enumeration.
enum WasmEdge_GcSubType {
#define UseGcSubType
#define Line(NAME, VALUE, STRING) WasmEdge_GcSubType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseGcSubType
};

/// WASM GC proposal defined type type C enumeration.
enum WasmEdge_GcDefinedType {
#define UseGcDefinedType
#define Line(NAME, VALUE, STRING) WasmEdge_GcDefinedType_##NAME = VALUE,
#include "enum.inc"
#undef Line
#undef UseGcDefinedType
};

#endif // WASMEDGE_C_API_ENUM_TYPES_H
