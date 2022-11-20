// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

#include "vm/vm.h"
#include "vm/async.h"

#include "host/wasi/wasimodule.h"
#include "plugin/plugin.h"

namespace WasmEdge {
namespace VM {

VM::VM(const Configure &Conf)
    : Conf(Conf), Stage(VMStage::Inited),
      LoaderEngine(Conf, &Executor::Executor::Intrinsics),
      ValidatorEngine(Conf), ExecutorEngine(Conf, &Stat),
      Store(std::make_unique<Runtime::StoreManager>()), StoreRef(*Store.get()) {
  unsafeInitVM();
}

VM::VM(const Configure &Conf, Runtime::StoreManager &S)
    : Conf(Conf), Stage(VMStage::Inited),
      LoaderEngine(Conf, &Executor::Executor::Intrinsics),
      ValidatorEngine(Conf), ExecutorEngine(Conf, &Stat), StoreRef(S) {
  unsafeInitVM();
}

void VM::unsafeInitVM() {
  using namespace std::literals::string_view_literals;
  // Create import modules from configuration.
  if (Conf.hasHostRegistration(HostRegistration::Wasi)) {
    std::unique_ptr<Runtime::Instance::ModuleInstance> WasiMod =
        std::make_unique<Host::WasiModule>();
    ExecutorEngine.registerModule(StoreRef, *WasiMod.get());
    ImpObjs.insert({HostRegistration::Wasi, std::move(WasiMod)});
  }

  // Load the plugins.
  auto loadPlugin = [=](std::string_view PName, HostRegistration Host,
                        std::string_view MName) {
    if (Conf.hasHostRegistration(Host)) {
      bool Founded = false;
      if (const auto *Plugin = Plugin::Plugin::find(PName)) {
        if (const auto *Module = Plugin->findModule(MName)) {
          auto ProcMod = Module->create();
          ExecutorEngine.registerModule(StoreRef, *ProcMod);
          ImpObjs.emplace(Host, std::move(ProcMod));
          Founded = true;
        }
      }
      if (!Founded) {
        spdlog::debug("Plugin:"sv, PName, "module: "sv, MName,
                      "not founded."sv);
      }
    }
  };
  loadPlugin("wasmedge_process"sv, HostRegistration::WasmEdge_Process,
             "wasmedge_process"sv);
  loadPlugin("wasi_nn"sv, HostRegistration::WasiNN, "wasi_nn"sv);
  loadPlugin("wasi_crypto"sv, HostRegistration::WasiCrypto_Common,
             "wasi_crypto_common"sv);
  loadPlugin("wasi_crypto"sv, HostRegistration::WasiCrypto_AsymmetricCommon,
             "wasi_crypto_asymmetric_common"sv);
  loadPlugin("wasi_crypto"sv, HostRegistration::WasiCrypto_Kx,
             "wasi_crypto_kx"sv);
  loadPlugin("wasi_crypto"sv, HostRegistration::WasiCrypto_Signatures,
             "wasi_crypto_signatures"sv);
  loadPlugin("wasi_crypto"sv, HostRegistration::WasiCrypto_Symmetric,
             "wasi_crypto_symmetric"sv);

  uint8_t Index = static_cast<uint8_t>(HostRegistration::Max);
  for (const auto &Plugin : Plugin::Plugin::plugins()) {
    if (Conf.isForbiddenPlugins(Plugin.name())) {
      continue;
    }
    // skip WasmEdge_Process, wasi_nn, ans wasi_crypto.
    if (Plugin.name() == "wasmedge_process"sv || Plugin.name() == "wasi_nn"sv ||
        Plugin.name() == "wasi_crypto"sv) {
      continue;
    }
    for (const auto &Module : Plugin.modules()) {
      auto ModObj = Module.create();
      ExecutorEngine.registerModule(StoreRef, *ModObj);
      ImpObjs.emplace(static_cast<HostRegistration>(Index++),
                      std::move(ModObj));
    }
  }
}

Expect<void> VM::unsafeRegisterModule(std::string_view Name,
                                      const std::filesystem::path &Path) {
  if (Stage == VMStage::Instantiated) {
    // When registering module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  // Load module.
  if (auto Res = LoaderEngine.parseModule(Path)) {
    return unsafeRegisterModule(Name, *(*Res).get());
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::unsafeRegisterModule(std::string_view Name,
                                      Span<const Byte> Code) {
  if (Stage == VMStage::Instantiated) {
    // When registering module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  // Load module.
  if (auto Res = LoaderEngine.parseModule(Code)) {
    return unsafeRegisterModule(Name, *(*Res).get());
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::unsafeRegisterModule(std::string_view Name,
                                      const AST::Module &Module) {
  if (Stage == VMStage::Instantiated) {
    // When registering module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  // Validate module.
  if (auto Res = ValidatorEngine.validate(Module); !Res) {
    return Unexpect(Res);
  }
  // Instantiate and register module.
  if (auto Res = ExecutorEngine.registerModule(StoreRef, Module, Name)) {
    RegModInst.push_back(std::move(*Res));
    return {};
  } else {
    return Unexpect(Res);
  }
}

Expect<void>
VM::unsafeRegisterModule(const Runtime::Instance::ModuleInstance &ModInst) {
  if (Stage == VMStage::Instantiated) {
    // When registering module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  return ExecutorEngine.registerModule(StoreRef, ModInst);
}

Expect<std::vector<std::pair<ValVariant, FullValType>>>
VM::unsafeRunWasmFile(const std::filesystem::path &Path, std::string_view Func,
                      Span<const ValVariant> Params,
                      Span<const FullValType> ParamTypes) {
  if (Stage == VMStage::Instantiated) {
    // When running another module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  // Load module.
  if (auto Res = LoaderEngine.parseModule(Path)) {
    return unsafeRunWasmFile(*(*Res).get(), Func, Params, ParamTypes);
  } else {
    return Unexpect(Res);
  }
}

Expect<std::vector<std::pair<ValVariant, FullValType>>>
VM::unsafeRunWasmFile(Span<const Byte> Code, std::string_view Func,
                      Span<const ValVariant> Params,
                      Span<const FullValType> ParamTypes) {
  if (Stage == VMStage::Instantiated) {
    // When running another module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  // Load module.
  if (auto Res = LoaderEngine.parseModule(Code)) {
    return unsafeRunWasmFile(*(*Res).get(), Func, Params, ParamTypes);
  } else {
    return Unexpect(Res);
  }
}

Expect<std::vector<std::pair<ValVariant, FullValType>>>
VM::unsafeRunWasmFile(const AST::Module &Module, std::string_view Func,
                      Span<const ValVariant> Params,
                      Span<const FullValType> ParamTypes) {
  if (Stage == VMStage::Instantiated) {
    // When running another module, instantiated module in store will be reset.
    // Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  if (auto Res = ValidatorEngine.validate(Module); !Res) {
    return Unexpect(Res);
  }
  if (auto Res = ExecutorEngine.instantiateModule(StoreRef, Module)) {
    ActiveModInst = std::move(*Res);
  } else {
    return Unexpect(Res);
  }
  // Get module instance.
  if (ActiveModInst) {
    // Execute function and return values with the module instance.
    return unsafeExecute(ActiveModInst.get(), Func, Params, ParamTypes);
  } else {
    spdlog::error(ErrCode::Value::WrongInstanceAddress);
    spdlog::error(ErrInfo::InfoExecuting("", Func));
    return Unexpect(ErrCode::Value::WrongInstanceAddress);
  }
}

Async<Expect<std::vector<std::pair<ValVariant, FullValType>>>>
VM::asyncRunWasmFile(const std::filesystem::path &Path, std::string_view Func,
                     Span<const ValVariant> Params,
                     Span<const FullValType> ParamTypes) {
  Expect<std::vector<std::pair<ValVariant, FullValType>>> (VM::*FPtr)(
      const std::filesystem::path &, std::string_view, Span<const ValVariant>,
      Span<const FullValType>) = &VM::runWasmFile;
  return {FPtr,
          *this,
          std::filesystem::path(Path),
          std::string(Func),
          std::vector(Params.begin(), Params.end()),
          std::vector(ParamTypes.begin(), ParamTypes.end())};
}

Async<Expect<std::vector<std::pair<ValVariant, FullValType>>>>
VM::asyncRunWasmFile(Span<const Byte> Code, std::string_view Func,
                     Span<const ValVariant> Params,
                     Span<const FullValType> ParamTypes) {
  Expect<std::vector<std::pair<ValVariant, FullValType>>> (VM::*FPtr)(
      Span<const Byte>, std::string_view, Span<const ValVariant>,
      Span<const FullValType>) = &VM::runWasmFile;
  return {FPtr,
          *this,
          Code,
          std::string(Func),
          std::vector(Params.begin(), Params.end()),
          std::vector(ParamTypes.begin(), ParamTypes.end())};
}

Async<Expect<std::vector<std::pair<ValVariant, FullValType>>>>
VM::asyncRunWasmFile(const AST::Module &Module, std::string_view Func,
                     Span<const ValVariant> Params,
                     Span<const FullValType> ParamTypes) {
  Expect<std::vector<std::pair<ValVariant, FullValType>>> (VM::*FPtr)(
      const AST::Module &, std::string_view, Span<const ValVariant>,
      Span<const FullValType>) = &VM::runWasmFile;
  return {FPtr,
          *this,
          Module,
          std::string(Func),
          std::vector(Params.begin(), Params.end()),
          std::vector(ParamTypes.begin(), ParamTypes.end())};
}

Expect<void> VM::unsafeLoadWasm(const std::filesystem::path &Path) {
  // If not load successfully, the previous status will be reserved.
  if (auto Res = LoaderEngine.parseModule(Path)) {
    Mod = std::move(*Res);
    Stage = VMStage::Loaded;
  } else {
    return Unexpect(Res);
  }
  return {};
}

Expect<void> VM::unsafeLoadWasm(Span<const Byte> Code) {
  // If not load successfully, the previous status will be reserved.
  if (auto Res = LoaderEngine.parseModule(Code)) {
    Mod = std::move(*Res);
    Stage = VMStage::Loaded;
  } else {
    return Unexpect(Res);
  }
  return {};
}

Expect<void> VM::unsafeLoadWasm(const AST::Module &Module) {
  Mod = std::make_unique<AST::Module>(Module);
  Stage = VMStage::Loaded;
  return {};
}

Expect<void> VM::unsafeValidate() {
  if (Stage < VMStage::Loaded) {
    // When module is not loaded, not validate.
    spdlog::error(ErrCode::Value::WrongVMWorkflow);
    return Unexpect(ErrCode::Value::WrongVMWorkflow);
  }
  if (auto Res = ValidatorEngine.validate(*Mod.get())) {
    Stage = VMStage::Validated;
    return {};
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::unsafeInstantiate() {
  if (Stage < VMStage::Validated) {
    // When module is not validated, not instantiate.
    spdlog::error(ErrCode::Value::WrongVMWorkflow);
    return Unexpect(ErrCode::Value::WrongVMWorkflow);
  }
  if (auto Res = ExecutorEngine.instantiateModule(StoreRef, *Mod.get())) {
    Stage = VMStage::Instantiated;
    ActiveModInst = std::move(*Res);
    return {};
  } else {
    return Unexpect(Res);
  }
}

Expect<std::vector<std::pair<ValVariant, FullValType>>>
VM::unsafeExecute(std::string_view Func, Span<const ValVariant> Params,
                  Span<const FullValType> ParamTypes) {
  if (ActiveModInst) {
    // Execute function and return values with the module instance.
    return unsafeExecute(ActiveModInst.get(), Func, Params, ParamTypes);
  } else {
    spdlog::error(ErrCode::Value::WrongInstanceAddress);
    spdlog::error(ErrInfo::InfoExecuting("", Func));
    return Unexpect(ErrCode::Value::WrongInstanceAddress);
  }
}

Expect<std::vector<std::pair<ValVariant, FullValType>>>
VM::unsafeExecute(std::string_view ModName, std::string_view Func,
                  Span<const ValVariant> Params,
                  Span<const FullValType> ParamTypes) {
  // Find module instance by name.
  const auto *FindModInst = StoreRef.findModule(ModName);
  if (FindModInst != nullptr) {
    // Execute function and return values with the module instance.
    return unsafeExecute(FindModInst, Func, Params, ParamTypes);
  } else {
    spdlog::error(ErrCode::Value::WrongInstanceAddress);
    spdlog::error(ErrInfo::InfoExecuting(ModName, Func));
    return Unexpect(ErrCode::Value::WrongInstanceAddress);
  }
}

Expect<std::vector<std::pair<ValVariant, FullValType>>>
VM::unsafeExecute(const Runtime::Instance::ModuleInstance *ModInst,
                  std::string_view Func, Span<const ValVariant> Params,
                  Span<const FullValType> ParamTypes) {
  // Find exported function by name.
  Runtime::Instance::FunctionInstance *FuncInst =
      ModInst->findFuncExports(Func);
  if (unlikely(FuncInst == nullptr)) {
    spdlog::error(ErrCode::Value::FuncNotFound);
    spdlog::error(ErrInfo::InfoExecuting(ModInst->getModuleName(), Func));
    return Unexpect(ErrCode::Value::FuncNotFound);
  }

  // Execute function.
  if (auto Res = ExecutorEngine.invoke(*FuncInst, Params, ParamTypes);
      unlikely(!Res)) {
    if (Res.error() != ErrCode::Value::Terminated) {
      spdlog::error(ErrInfo::InfoExecuting(ModInst->getModuleName(), Func));
    }
    return Unexpect(Res);
  } else {
    return Res;
  }
}

Async<Expect<std::vector<std::pair<ValVariant, FullValType>>>>
VM::asyncExecute(std::string_view Func, Span<const ValVariant> Params,
                 Span<const FullValType> ParamTypes) {
  Expect<std::vector<std::pair<ValVariant, FullValType>>> (VM::*FPtr)(
      std::string_view, Span<const ValVariant>, Span<const FullValType>) =
      &VM::execute;
  return {FPtr, *this, std::string(Func),
          std::vector(Params.begin(), Params.end()),
          std::vector(ParamTypes.begin(), ParamTypes.end())};
}

Async<Expect<std::vector<std::pair<ValVariant, FullValType>>>>
VM::asyncExecute(std::string_view ModName, std::string_view Func,
                 Span<const ValVariant> Params,
                 Span<const FullValType> ParamTypes) {
  Expect<std::vector<std::pair<ValVariant, FullValType>>> (VM::*FPtr)(
      std::string_view, std::string_view, Span<const ValVariant>,
      Span<const FullValType>) = &VM::execute;
  return {FPtr,
          *this,
          std::string(ModName),
          std::string(Func),
          std::vector(Params.begin(), Params.end()),
          std::vector(ParamTypes.begin(), ParamTypes.end())};
}

void VM::unsafeCleanup() {
  Mod.reset();
  ActiveModInst.reset();
  Stat.clear();
  Stage = VMStage::Inited;
}

std::vector<std::pair<std::string, const AST::FunctionType &>>
VM::unsafeGetFunctionList() const {
  std::vector<std::pair<std::string, const AST::FunctionType &>> Map;
  if (ActiveModInst) {
    ActiveModInst->getFuncExports([&](const auto &FuncExports) {
      Map.reserve(FuncExports.size());
      for (auto &&Func : FuncExports) {
        const auto &FuncType = (Func.second)->getFuncType();
        Map.emplace_back(Func.first, FuncType);
      }
    });
  }
  return Map;
}

Runtime::Instance::ModuleInstance *
VM::unsafeGetImportModule(const HostRegistration Type) const {
  if (auto Iter = ImpObjs.find(Type); Iter != ImpObjs.cend()) {
    return Iter->second.get();
  }
  return nullptr;
}

const Runtime::Instance::ModuleInstance *VM::unsafeGetActiveModule() const {
  if (ActiveModInst) {
    return ActiveModInst.get();
  }
  return nullptr;
};

} // namespace VM
} // namespace WasmEdge
