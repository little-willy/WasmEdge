// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

#include "loader/loader.h"

#include <cstdint>

namespace WasmEdge {
namespace Loader {

Expect<void> Loader::loadLimit(AST::Limit &Lim) {
  // Read limit.
  if (auto Res = FMgr.readByte()) {

    switch (static_cast<AST::Limit::LimitType>(*Res)) {
    case AST::Limit::LimitType::HasMin:
      Lim.setType(AST::Limit::LimitType::HasMin);
      break;
    case AST::Limit::LimitType::HasMinMax:
      Lim.setType(AST::Limit::LimitType::HasMinMax);
      break;
    case AST::Limit::LimitType::SharedNoMax:
      if (Conf.hasProposal(Proposal::Threads)) {
        return logLoadError(ErrCode::Value::SharedMemoryNoMax,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      } else {
        return logLoadError(ErrCode::Value::IntegerTooLarge,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      }
    case AST::Limit::LimitType::Shared:
      Lim.setType(AST::Limit::LimitType::Shared);
      break;
    default:
      if (*Res == 0x80 || *Res == 0x81) {
        // LEB128 cases will fail.
        return logLoadError(ErrCode::Value::IntegerTooLong,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      } else {
        return logLoadError(ErrCode::Value::IntegerTooLarge,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      }
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Limit);
  }

  // Read min and max number.
  if (auto Res = FMgr.readU32()) {
    Lim.setMin(*Res);
    Lim.setMax(*Res);
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Limit);
  }
  if (Lim.hasMax()) {
    if (auto Res = FMgr.readU32()) {
      Lim.setMax(*Res);
    } else {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Limit);
    }
  }
  return {};
}

Expect<void> Loader::loadType(AST::DefinedType &DefinedType) {
  u_int8_t OpCode = 0;

  // Read function type (0x60).
  if (auto Res = FMgr.readByte()) {
    OpCode = *Res;
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }

  switch (OpCode) {
  case (uint8_t)GcDefinedType::TypeFunc: {
    AST::FunctionType FuncType;
    if (auto Res = loadType(FuncType); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
    DefinedType = AST::DefinedType(std::move(FuncType));
    break;
  }
  case (uint8_t)GcDefinedType::TypeStruct: {
    AST::StructType StructType;
    if (auto Res = loadType(StructType); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }

    DefinedType = AST::DefinedType(std::move(StructType));
    break;
  }
  case (uint8_t)GcDefinedType::TypeArray: {
    AST::ArrayType ArrayType;

    if (auto Res = loadType(ArrayType); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }

    DefinedType = AST::DefinedType(std::move(ArrayType));
    break;
  }
  case (uint8_t)GcDefinedType::TypeSub: {
    AST::SubType SubType;

    if (auto Res = loadType(SubType); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }

    DefinedType = AST::DefinedType(std::vector({SubType}));
    break;
  }
  case (uint8_t)GcDefinedType::TypeRec: {
    std::vector<AST::SubType> SubTypes;

    if (auto Res = loadVec(
            SubTypes,
            [this](AST::SubType &SubType) { return loadType(SubType); });
        !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }

    DefinedType = AST::DefinedType(std::move(SubTypes));
    break;
  }
  default: {
    return logLoadError(ErrCode::Value::IntegerTooLong, FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  }

  return {};
}

Expect<void> Loader::loadType(AST::ArrayType &ArrayType) {
  AST::FieldType FieldType;
  if (auto Res = loadType(FieldType); !Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  ArrayType = AST::ArrayType(FieldType);
  return {};
}

Expect<void> Loader::loadType(AST::StructType &StructType) {
  std::vector<AST::FieldType> FieldTypes;

  if (auto Res = loadVec(
          FieldTypes,
          [this](AST::FieldType &FieldType) { return loadType(FieldType); });
      !Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }

  StructType = AST::StructType(std::move(FieldTypes));
  return {};
}

Expect<void> Loader::loadType(AST::SubType &SubType) {
  uint32_t VecCnt = 0;
  std::vector<uint32_t> ParantIdxList;

  // Read vector of parameter types.
  if (auto Res = FMgr.readU32()) {
    VecCnt = *Res;
    ParantIdxList.reserve(VecCnt);
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  for (uint32_t I = 0; I < VecCnt; ++I) {
    if (auto Res = FMgr.readByte()) {
      ParantIdxList.push_back(*Res);
    } else {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
  }

  AST::StructureType StructureType;
  if (auto Res = loadType(StructureType); !Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  SubType = AST::SubType(std::move(ParantIdxList), std::move(StructureType));
  return {};
}

Expect<void> Loader::loadType(AST::StructureType &StructureType) {
  u_int8_t OpCode = 0;

  // Read function type (0x60).
  if (auto Res = FMgr.readByte()) {
    OpCode = *Res;
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }

  switch (OpCode) {
  case (uint8_t)GcStructureType::Array: {
    AST::ArrayType Type;
    if (auto Res = loadType(Type); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
    StructureType = AST::StructureType(std::move(Type));
    break;
  }
  case (uint8_t)GcStructureType::Struct: {
    AST::StructType Type;
    if (auto Res = loadType(Type); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
    StructureType = AST::StructureType(std::move(Type));
    break;
  }
  case (uint8_t)GcStructureType::Func: {
    AST::FunctionType Type;
    if (auto Res = loadType(Type); !Res) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
    StructureType = AST::StructureType(std::move(Type));
    break;
  }
  default: {
    return logLoadError(ErrCode::Value::IntegerTooLong, FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  }
  return {};
}

Expect<void> Loader::loadType(AST::FieldType &FieldType) {
  uint8_t Mutability;
  uint8_t StorageType;

  if (auto Res = FMgr.readByte()) {
    Mutability = *Res;
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }

  if (auto Res = FMgr.readByte()) {
    StorageType = *Res;
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }

  FieldType = AST::FieldType(static_cast<ValMut>(Mutability),
                             static_cast<GcStorageType>(StorageType));
  return {};
}

// Load binary to construct FunctionType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::FunctionType &FuncType) {
  uint32_t VecCnt = 0;

  // Read vector of parameter types.
  if (auto Res = FMgr.readU32()) {
    VecCnt = *Res;
    FuncType.getParamTypes().clear();
    FuncType.getParamTypes().reserve(VecCnt);
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  for (uint32_t I = 0; I < VecCnt; ++I) {
    if (auto Res = FMgr.readByte()) {
      ValType Type = static_cast<ValType>(*Res);
      if (auto Check = checkValTypeProposals(Type, false, FMgr.getLastOffset(),
                                             ASTNodeAttr::Type_Function);
          !Check) {
        return Unexpect(Check);
      }
      FuncType.getParamTypes().push_back(Type);
    } else {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
  }

  // Read vector of result types.
  if (auto Res = FMgr.readU32()) {
    VecCnt = *Res;
    FuncType.getReturnTypes().clear();
    FuncType.getReturnTypes().reserve(VecCnt);
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Function);
  }
  if (unlikely(!Conf.hasProposal(Proposal::MultiValue)) && VecCnt > 1) {
    return logNeedProposal(ErrCode::Value::MalformedValType,
                           Proposal::MultiValue, FMgr.getLastOffset(),
                           ASTNodeAttr::Type_Function);
  }
  for (uint32_t I = 0; I < VecCnt; ++I) {
    if (auto Res = FMgr.readByte()) {
      ValType Type = static_cast<ValType>(*Res);
      if (auto Check = checkValTypeProposals(Type, false, FMgr.getLastOffset(),
                                             ASTNodeAttr::Type_Function);
          !Check) {
        return Unexpect(Check);
      }
      FuncType.getReturnTypes().push_back(Type);
    } else {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Function);
    }
  }
  return {};
}

// Load binary to construct MemoryType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::MemoryType &MemType) {
  // Read limit.
  if (auto Res = loadLimit(MemType.getLimit()); !Res) {
    spdlog::error(ErrInfo::InfoAST(ASTNodeAttr::Type_Memory));
    return Unexpect(Res);
  }
  return {};
}

// Load binary to construct TableType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::TableType &TabType) {
  // Read reference type.
  if (auto Res = FMgr.readByte()) {
    TabType.setRefType(static_cast<RefType>(*Res));
    if (auto Check =
            checkRefTypeProposals(TabType.getRefType(), FMgr.getLastOffset(),
                                  ASTNodeAttr::Type_Table);
        !Check) {
      return Unexpect(Check);
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Table);
  }

  // Read limit.
  if (auto Res = loadLimit(TabType.getLimit()); !Res) {
    spdlog::error(ErrInfo::InfoAST(ASTNodeAttr::Type_Table));
    return Unexpect(Res);
  }
  return {};
}

// Load binary to construct GlobalType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::GlobalType &GlobType) {
  // Read value type.
  if (auto Res = FMgr.readByte()) {
    GlobType.setValType(static_cast<ValType>(*Res));
    if (auto Check = checkValTypeProposals(GlobType.getValType(), false,
                                           FMgr.getLastOffset(),
                                           ASTNodeAttr::Type_Global);
        !Check) {
      return Unexpect(Check);
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Global);
  }

  // Read mutability.
  if (auto Res = FMgr.readByte()) {
    GlobType.setValMut(static_cast<ValMut>(*Res));
    switch (GlobType.getValMut()) {
    case ValMut::Const:
    case ValMut::Var:
      break;
    default:
      return logLoadError(ErrCode::Value::InvalidMut, FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Global);
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Global);
  }
  return {};
}

} // namespace Loader
} // namespace WasmEdge
