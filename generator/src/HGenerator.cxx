#include "generator/HGenerator.hxx"
#include <ast/HAstNode.hxx>
#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/CodeGen/TargetRegisterInfo.h>
#include <llvm/TargetParser/TargetParser.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Option/Option.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Linker/Linker.h>
#include <filesystem>
#include <optional>

using namespace llvm;

#ifdef HYVE_BOOTSTRAP
std::unique_ptr<Module> LoadLowLevelStandardLibrary(llvm::LLVMContext& context) {
    // Load the low level standard library LLVM IR code and link it with the current module
    SMDiagnostic err;
    auto path = std::filesystem::current_path() / "low_level.bc";
    auto mod = parseIRFile(path.string(), err, context);
    if (!mod) {
        err.print("HyveCompiler", errs());
        return nullptr;
    }

    return mod;
}

void LinkStandardLibrary(llvm::LLVMContext& context, llvm::Module* currentModule) {
    auto lowLevelModule = LoadLowLevelStandardLibrary(context);
    if (!lowLevelModule) {
        return;
    }

    // Link the low level standard library with the current module
    if (Linker::linkModules(*currentModule, std::move(lowLevelModule))) {
        errs() << "Error linking modules\n";
        return;
    }

    // Print the module to stdout
    currentModule->print(outs(), nullptr);
}
#endif

namespace Hyve::Generator {
    using namespace AST;

	std::string HGenerator::GenerateIR(std::string_view fileName, std::shared_ptr<HAstNode> nodes) const {
        // Initialize the target registry etc.
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();

        // Create a context to hold the LLVM constructs
        LLVMContext context;

        // Create a new module
        auto currentModule = std::make_unique<Module>(fileName, context);

        // Create a function type: int main()
        FunctionType* mainType = FunctionType::get(Type::getInt32Ty(context), false);

        // Create the main function: int main()
        Function* mainFunction = Function::Create(mainType, Function::ExternalLinkage, "main", *currentModule);

        // Create a basic block and attach it to the main function
        BasicBlock* entryBlock = BasicBlock::Create(context, "entry", mainFunction);

        // Create an IR builder and set the insertion point to the entry block
        IRBuilder<> builder(entryBlock);

        // Get the type for printf: int printf(const char *format, ...)
        PointerType* printfArgType = PointerType::get(Type::getInt8Ty(context), 0);
        FunctionType* printfType = FunctionType::get(builder.getInt32Ty(), printfArgType, true);

        // Create the printf function and add it to the module
        FunctionCallee printf = currentModule->getOrInsertFunction("printf", printfType);

        std::vector<Type*> funcArgTypes(2, Type::getInt32Ty(context));
        FunctionType* externCFuncType = FunctionType::get(Type::getInt32Ty(context), funcArgTypes, false);

        // Create the external C function: int CFunc(int, int)
        Function* externCFunc = Function::Create(externCFuncType, Function::ExternalLinkage, "CFunc", *currentModule);
        externCFunc->setCallingConv(CallingConv::C);

        auto* arg1 = builder.getInt32(2);
        auto* arg2 = builder.getInt32(0);
        // Call the function: CFunc(2, 0)
        auto* callResult = builder.CreateCall(externCFunc, { arg1, arg2 });

        // Create two basic blocks for conditional branching
        auto* evenBlock = BasicBlock::Create(context, "even", mainFunction);
        auto* oddBlock = BasicBlock::Create(context, "odd", mainFunction);

        // Create the conditional branch
        auto* modResult = builder.CreateSRem(callResult, builder.getInt32(2));
        auto* isOdd = builder.CreateICmpNE(modResult, builder.getInt32(0));
        builder.CreateCondBr(isOdd, oddBlock, evenBlock);

        // Set the insertion point to the even block
        builder.SetInsertPoint(evenBlock);
        auto* evenString = builder.CreateGlobalStringPtr("Even\n");
        builder.CreateCall(printf, { evenString });
        builder.CreateRet(builder.getInt32(0));

        // Set the insertion point to the odd block
        builder.SetInsertPoint(oddBlock);
        auto* oddString = builder.CreateGlobalStringPtr("Odd\n");
        builder.CreateCall(printf, { oddString });
        builder.CreateRet(builder.getInt32(0));

        // Print the module to stdout
        currentModule->print(outs(), nullptr);

        // Set up the target machine
        std::string targetTriple = sys::getDefaultTargetTriple();
        currentModule->setTargetTriple(targetTriple);
        std::string Error;
        const Target* Target = TargetRegistry::lookupTarget(targetTriple, Error);

        if (!Target) {
            errs() << "Error: " << Error;
            return "";
        }

        TargetOptions opt;
        auto RM = std::optional<Reloc::Model>();
        auto targetMachine = std::unique_ptr<TargetMachine>(
            Target->createTargetMachine(targetTriple, "generic", "", opt, RM)
        );

        currentModule->setDataLayout(targetMachine->createDataLayout());

#ifdef HYVE_BOOTSTRAP
        // Link the standard library
        LinkStandardLibrary(context, currentModule.get());
#endif

        std::error_code EC;
        raw_fd_ostream dest("hello.o", EC, sys::fs::OF_None);

        if (EC) {
            errs() << "Could not open file: " << EC.message();
            return "";
        }

        legacy::PassManager pass;
        if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, CodeGenFileType::ObjectFile)) {
            errs() << "TargetMachine can't emit a file of this type";
            return "";
        }

        pass.run(*currentModule);
        dest.flush();

        outs() << "Object file generated successfully\n";

        return "";
	}
}