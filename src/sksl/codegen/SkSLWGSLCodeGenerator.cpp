/*
 * Copyright 2022 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/codegen/SkSLWGSLCodeGenerator.h"

#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLStringStream.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/ir/SkSLBinaryExpression.h"
#include "src/sksl/ir/SkSLBlock.h"
#include "src/sksl/ir/SkSLConstructor.h"
#include "src/sksl/ir/SkSLConstructorCompound.h"
#include "src/sksl/ir/SkSLExpression.h"
#include "src/sksl/ir/SkSLExpressionStatement.h"
#include "src/sksl/ir/SkSLFunctionDeclaration.h"
#include "src/sksl/ir/SkSLFunctionDefinition.h"
#include "src/sksl/ir/SkSLInterfaceBlock.h"
#include "src/sksl/ir/SkSLLiteral.h"
#include "src/sksl/ir/SkSLReturnStatement.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"

// TODO(skia:13092): This is a temporary debug feature. Remove when the implementation is
// complete and this is no longer needed.
#define DUMP_SRC_IR 0

namespace SkSL {
namespace {

// See https://www.w3.org/TR/WGSL/#memory-view-types
enum class PtrAddressSpace {
    kFunction,
    kPrivate,
    kStorage,
};

std::string address_space_to_str(PtrAddressSpace addressSpace) {
    switch (addressSpace) {
        case PtrAddressSpace::kFunction:
            return "function";
        case PtrAddressSpace::kPrivate:
            return "private";
        case PtrAddressSpace::kStorage:
            return "storage";
    }
    SkDEBUGFAIL("unsupported ptr address space");
    return "unsupported";
}

std::string to_scalar_type(const Type& type) {
    SkASSERT(type.typeKind() == Type::TypeKind::kScalar);
    switch (type.numberKind()) {
        // Floating-point numbers in WebGPU currently always have 32-bit footprint and
        // relaxed-precision is not supported without extensions. f32 is the only floating-point
        // number type in WGSL (see the discussion on https://github.com/gpuweb/gpuweb/issues/658).
        case Type::NumberKind::kFloat:
            return "f32";
        case Type::NumberKind::kSigned:
            return "i32";
        case Type::NumberKind::kUnsigned:
            return "u32";
        case Type::NumberKind::kBoolean:
            return "bool";
        case Type::NumberKind::kNonnumeric:
            [[fallthrough]];
        default:
            break;
    }
    return std::string(type.name());
}

// Convert a SkSL type to a WGSL type. Handles all plain types except structure types
// (see https://www.w3.org/TR/WGSL/#plain-types-section).
std::string to_wgsl_type(const Type& type) {
    // TODO(skia:13092): Handle array, matrix, sampler types.
    switch (type.typeKind()) {
        case Type::TypeKind::kScalar:
            return to_scalar_type(type);
        case Type::TypeKind::kVector:
            return "vec" + std::to_string(type.columns()) + "<" +
                   to_scalar_type(type.componentType()) + ">";
        default:
            break;
    }
    return std::string(type.name());
}

std::string to_ptr_type(const Type& type,
                        PtrAddressSpace addressSpace = PtrAddressSpace::kFunction) {
    return "ptr<" + address_space_to_str(addressSpace) + ", " + to_wgsl_type(type) + ">";
}

std::string to_wgsl_builtin_name(WGSLCodeGenerator::Builtin kind) {
    using Builtin = WGSLCodeGenerator::Builtin;
    switch (kind) {
        case Builtin::kVertexIndex:
            return "vertex_index";
        case Builtin::kInstanceIndex:
            return "instance_index";
        case Builtin::kPosition:
            return "position";
        case Builtin::kFrontFacing:
            return "front_facing";
        case Builtin::kSampleIndex:
            return "sample_index";
        case Builtin::kFragDepth:
            return "frag_depth";
        case Builtin::kSampleMask:
            return "sample_mask";
        case Builtin::kLocalInvocationId:
            return "local_invocation_id";
        case Builtin::kLocalInvocationIndex:
            return "local_invocation_index";
        case Builtin::kGlobalInvocationId:
            return "global_invocation_id";
        case Builtin::kWorkgroupId:
            return "workgroup_id";
        case Builtin::kNumWorkgroups:
            return "num_workgroups";
        default:
            break;
    }

    SkDEBUGFAIL("unsupported builtin");
    return "unsupported";
}

// Map a SkSL builtin flag to a WGSL builtin kind. Returns std::nullopt if `builtin` is not
// not supported for WGSL.
//
// Also see //src/sksl/sksl_vert.sksl and //src/sksl/sksl_frag.sksl for supported built-ins.
std::optional<WGSLCodeGenerator::Builtin> builtin_from_sksl_name(int builtin) {
    using Builtin = WGSLCodeGenerator::Builtin;
    switch (builtin) {
        case SK_POSITION_BUILTIN:
            [[fallthrough]];
        case SK_FRAGCOORD_BUILTIN:
            return {Builtin::kPosition};
        case SK_VERTEXID_BUILTIN:
            return {Builtin::kVertexIndex};
        case SK_INSTANCEID_BUILTIN:
            return {Builtin::kInstanceIndex};
        case SK_CLOCKWISE_BUILTIN:
            // TODO(skia:13092): While `front_facing` is the corresponding built-in, it does not
            // imply a particular winding order. We correctly compute the face orientation based
            // on how Skia configured the render pipeline for all references to this built-in
            // variable (see `SkSL::Program::Inputs::fUseFlipRTUniform`).
            return {Builtin::kFrontFacing};
        default:
            break;
    }
    return std::nullopt;
}

std::shared_ptr<SymbolTable> top_level_symbol_table(const FunctionDefinition& f) {
    return f.body()->as<Block>().symbolTable()->fParent;
}

}  // namespace

bool WGSLCodeGenerator::generateCode() {
    // The resources of a WGSL program are structured in the following way:
    // - Vertex and fragment stage attribute inputs and outputs are bundled
    //   inside synthetic structs called VSIn/VSOut/FSIn/FSOut.
    // - All uniform and storage type resources are declared in global scope.
    StringStream header;
    {
        AutoOutputStream outputToHeader(this, &header, &fIndentation);
        // TODO(skia:13092): Implement the following:
        // - struct definitions
        // - global uniform/storage resource declarations, including interface blocks.
        this->writeStageInputStruct();
        this->writeStageOutputStruct();
    }
    StringStream body;
    {
        AutoOutputStream outputToBody(this, &body, &fIndentation);
        for (const ProgramElement* e : fProgram.elements()) {
            this->writeProgramElement(*e);
        }

// TODO(skia:13092): This is a temporary debug feature. Remove when the implementation is
// complete and this is no longer needed.
#if DUMP_SRC_IR
        this->writeLine("\n----------");
        this->writeLine("Source IR:\n");
        for (const ProgramElement* e : fProgram.elements()) {
            this->writeLine(e->description().c_str());
        }
#endif
    }

    write_stringstream(header, *fOut);
    write_stringstream(body, *fOut);
    fContext.fErrors->reportPendingErrors(Position());
    return fContext.fErrors->errorCount() == 0;
}

void WGSLCodeGenerator::write(std::string_view s) {
    if (s.empty()) {
        return;
    }
    if (fAtLineStart) {
        for (int i = 0; i < fIndentation; i++) {
            fOut->writeText("    ");
        }
    }
    fOut->writeText(std::string(s).c_str());
    fAtLineStart = false;
}

void WGSLCodeGenerator::writeLine(std::string_view s) {
    this->write(s);
    fOut->writeText("\n");
    fAtLineStart = true;
}

void WGSLCodeGenerator::finishLine() {
    if (!fAtLineStart) {
        this->writeLine();
    }
}

void WGSLCodeGenerator::writeName(std::string_view name) {
    // Add underscore before name to avoid conflict with reserved words.
    if (fReservedWords.contains(name)) {
        this->write("_");
    }
    this->write(name);
}

void WGSLCodeGenerator::writePipelineIODeclaration(Modifiers modifiers,
                                                   const Type& type,
                                                   std::string_view name) {
    // In WGSL, an entry-point IO parameter is "one of either a built-in value or
    // assigned a location". However, some SkSL declarations, specifically sk_FragColor, can
    // contain both a location and a builtin modifier. In addition, WGSL doesn't have a built-in
    // equivalent for sk_FragColor as it relies on the user-defined location for a render
    // target.
    //
    // Instead of special-casing sk_FragColor, we just give higher precedence to a location
    // modifier if a declaration happens to both have a location and it's a built-in.
    //
    // Also see:
    // https://www.w3.org/TR/WGSL/#input-output-locations
    // https://www.w3.org/TR/WGSL/#attribute-location
    // https://www.w3.org/TR/WGSL/#builtin-inputs-outputs
    int location = modifiers.fLayout.fLocation;
    if (location >= 0) {
        this->writeUserDefinedVariableDecl(type, name, location);
    } else if (modifiers.fLayout.fBuiltin >= 0) {
        auto builtin = builtin_from_sksl_name(modifiers.fLayout.fBuiltin);
        if (builtin.has_value()) {
            this->writeBuiltinVariableDecl(type, name, *builtin);
        }
    }
}

void WGSLCodeGenerator::writeUserDefinedVariableDecl(const Type& type,
                                                     std::string_view name,
                                                     int location) {
    this->write("@location(" + std::to_string(location) + ") ");
    this->writeName(name);
    this->write(": " + to_wgsl_type(type));
    this->writeLine(";");
}

void WGSLCodeGenerator::writeBuiltinVariableDecl(const Type& type,
                                                 std::string_view name,
                                                 Builtin kind) {
    this->write("@builtin(" + to_wgsl_builtin_name(kind) + ") ");
    this->writeName(name);
    this->write(": " + to_wgsl_type(type));
    this->writeLine(";");
}

void WGSLCodeGenerator::writeFunction(const FunctionDefinition& f) {
    this->writeFunctionDeclaration(f.declaration());
    this->write(" ");
    this->writeBlock(f.body()->as<Block>());

    if (f.declaration().isMain()) {
        // We just emitted the user-defined main function. Next, we generate a program entry point
        // that calls the user-defined main.
        this->writeEntryPoint(f);
    }
}

void WGSLCodeGenerator::writeFunctionDeclaration(const FunctionDeclaration& f) {
    this->write("fn ");
    this->write(f.mangledName());

    // TODO(skia:13092): If a user-defined function references a pipeline stage input or output
    // parameter, then that need to be propagated as part of the stage input/output struct down
    // from the program entry point. We need to synthesize the necessary function arguments by
    // walking the function body beforehand to determine required parameters (similarly to how
    // SkSLMetalCodeGenerator does it).
    this->write("(");
    const char* separator = "";
    for (const Variable* param : f.parameters()) {
        this->write(separator);
        separator = ", ";
        this->writeName(param->name());
        this->write(": ");

        // Declare an "out" function parameter as a pointer.
        if (param->modifiers().fFlags & Modifiers::kOut_Flag) {
            this->write(to_ptr_type(param->type()));
        } else {
            this->write(to_wgsl_type(param->type()));
        }
    }
    this->write(")");

    if (!f.returnType().isVoid()) {
        this->write(" -> ");
        this->write(to_wgsl_type(f.returnType()));
    }
}

void WGSLCodeGenerator::writeEntryPoint(const FunctionDefinition& main) {
    SkASSERT(main.declaration().isMain());

    // The input and output parameters for a vertex/fragment stage entry point function have the
    // FSIn/FSOut/VSIn/VSOut struct types that have been synthesized in generateCode(). An entry
    // point always has the same signature and acts as a trampoline to the user-defined main
    // function.
    std::string outputType;
    if (fProgram.fConfig->fKind == ProgramKind::kVertex) {
        this->writeLine("@stage(vertex) fn vertexMain(_stageIn: VSIn) -> VSOut {");
        outputType = "VSOut";
    } else if (fProgram.fConfig->fKind == ProgramKind::kFragment) {
        this->writeLine("@stage(fragment) fn fragmentMain(_stageIn: FSIn) -> FSOut {");
        outputType = "FSOut";
    } else {
        fContext.fErrors->error(Position(), "program kind not supported");
        return;
    }

    // Declare the stage output struct.
    fIndentation++;
    this->write("var _stageOut: ");
    this->write(outputType);
    this->writeLine(";");

    // Generate the function call to the user-defined main:
    if (fProgram.fConfig->fKind == ProgramKind::kFragment) {
        auto symbolTable = top_level_symbol_table(main);
        const Symbol* symbol = (*symbolTable)["sk_FragColor"];
        SkASSERT(symbol);
        if (main.declaration().returnType().matches(symbol->type())) {
            this->write("_stageOut.sk_FragColor = ");
        }
    }

    this->write(main.declaration().mangledName());
    this->write("(");

    // TODO(skia:13092): Inject stage input/output struct if the function needs to access varying
    // variables.
    if (main.declaration().parameters().size() != 0) {
        const Type& type = main.declaration().parameters()[0]->type();
        if (!type.matches(*fContext.fTypes.fFloat2)) {
            fContext.fErrors->error(
                    main.fPosition,
                    "main function has unsupported parameter: " + type.description());
            return;
        }

        this->write("_stageIn.sk_FragCoord.xy");
    }

    this->writeLine(");");

    this->writeLine("return _stageOut;");
    fIndentation--;
    this->writeLine("}");
}

void WGSLCodeGenerator::writeStatement(const Statement& s) {
    switch (s.kind()) {
        case Statement::Kind::kBlock:
            this->writeBlock(s.as<Block>());
            break;
        case Statement::Kind::kExpression:
            this->writeExpressionStatement(s.as<ExpressionStatement>());
            break;
        case Statement::Kind::kReturn:
            this->writeReturnStatement(s.as<ReturnStatement>());
            break;
        case Statement::Kind::kVarDeclaration:
            this->writeVarDeclaration(s.as<VarDeclaration>());
            break;
        default:
            SkDEBUGFAILF("unsupported statement (kind: %d) %s", s.kind(), s.description().c_str());
            break;
    }
}

void WGSLCodeGenerator::writeStatements(const StatementArray& statements) {
    for (const auto& s : statements) {
        if (!s->isEmpty()) {
            this->writeStatement(*s);
            this->finishLine();
        }
    }
}

void WGSLCodeGenerator::writeBlock(const Block& b) {
    // Write scope markers if this block is a scope, or if the block is empty (since we need to emit
    // something here to make the code valid).
    bool isScope = b.isScope() || b.isEmpty();
    if (isScope) {
        this->writeLine("{");
        fIndentation++;
    }
    this->writeStatements(b.children());
    if (isScope) {
        fIndentation--;
        this->writeLine("}");
    }
}

void WGSLCodeGenerator::writeExpressionStatement(const ExpressionStatement& s) {
    if (s.expression()->hasSideEffects()) {
        this->writeExpression(*s.expression(), Precedence::kTopLevel);
        this->write(";");
    }
}

void WGSLCodeGenerator::writeReturnStatement(const ReturnStatement& s) {
    this->write("return");
    if (s.expression()) {
        this->write(" ");
        this->writeExpression(*s.expression(), Precedence::kTopLevel);
    }
    this->write(";");
}

void WGSLCodeGenerator::writeVarDeclaration(const VarDeclaration& varDecl) {
    bool isConst = varDecl.var().modifiers().fFlags & Modifiers::kConst_Flag;
    if (isConst) {
        this->write("let ");
    } else {
        this->write("var ");
    }
    this->writeName(varDecl.var().name());
    this->write(": ");
    this->write(to_wgsl_type(varDecl.var().type()));

    if (varDecl.value()) {
        this->write(" = ");
        this->writeExpression(*varDecl.value(), Precedence::kTopLevel);
    } else if (isConst) {
        SkDEBUGFAILF("A let-declared constant must specify a value");
    }

    this->write(";");
}

void WGSLCodeGenerator::writeExpression(const Expression& e, Precedence parentPrecedence) {
    switch (e.kind()) {
        case Expression::Kind::kBinary:
            this->writeBinaryExpression(e.as<BinaryExpression>(), parentPrecedence);
            break;
        case Expression::Kind::kConstructorCompound:
            this->writeConstructorCompound(e.as<ConstructorCompound>(), parentPrecedence);
            break;
        case Expression::Kind::kLiteral:
            this->writeLiteral(e.as<Literal>());
            break;
        default:
            SkDEBUGFAILF("unsupported expression (kind: %d) %s", e.kind(), e.description().c_str());
            break;
    }
}

void WGSLCodeGenerator::writeBinaryExpression(const BinaryExpression& b,
                                              Precedence parentPrecedence) {
    // TODO(skia:13092): implement
}

void WGSLCodeGenerator::writeLiteral(const Literal& l) {
    const Type& type = l.type();
    if (type.isFloat()) {
        this->write(skstd::to_string(l.floatValue()));
        return;
    }
    if (type.isBoolean()) {
        this->write(l.boolValue() ? "true" : "false");
        return;
    }
    SkASSERT(type.isInteger());
    if (type.matches(*fContext.fTypes.fUInt)) {
        this->write(std::to_string(l.intValue() & 0xffffffff));
        this->write("u");
    } else if (type.matches(*fContext.fTypes.fUShort)) {
        this->write(std::to_string(l.intValue() & 0xffff));
        this->write("u");
    } else {
        this->write(std::to_string(l.intValue()));
    }
}

void WGSLCodeGenerator::writeAnyConstructor(const AnyConstructor& c, Precedence parentPrecedence) {
    this->write(to_wgsl_type(c.type()));
    this->write("(");
    const char* separator = "";
    for (const auto& e : c.argumentSpan()) {
        this->write(separator);
        separator = ", ";
        this->writeExpression(*e, Precedence::kSequence);
    }
    this->write(")");
}

void WGSLCodeGenerator::writeConstructorCompound(const ConstructorCompound& c,
                                                 Precedence parentPrecedence) {
    // TODO(skia:13092): Support matrix constructors
    if (c.type().isVector()) {
        this->writeConstructorCompoundVector(c, parentPrecedence);
    } else {
        fContext.fErrors->error(c.fPosition, "unsupported compound constructor");
    }
}

void WGSLCodeGenerator::writeConstructorCompoundVector(const ConstructorCompound& c,
                                                       Precedence parentPrecedence) {
    // TODO(skia:13092): WGSL supports constructing vectors from a mix of scalars and vectors but
    // not matrices. SkSL supports vec4(mat2x2) which we need to handle here
    // (see https://www.w3.org/TR/WGSL/#type-constructor-expr).
    this->writeAnyConstructor(c, parentPrecedence);
}

void WGSLCodeGenerator::writeProgramElement(const ProgramElement& e) {
    switch (e.kind()) {
        case ProgramElement::Kind::kExtension:
            // TODO(skia:13092): WGSL supports extensions via the "enable" directive
            // (https://www.w3.org/TR/WGSL/#language-extensions). While we could easily emit this
            // directive, we should first ensure that all possible SkSL extension names are
            // converted to their appropriate WGSL extension. Currently there are no known supported
            // WGSL extensions aside from the hypotheticals listed in the spec.
            break;
        case ProgramElement::Kind::kGlobalVar:
            // All global declarations are handled explicitly as the "program header" in
            // generateCode().
            break;
        case ProgramElement::Kind::kInterfaceBlock:
            // All interface block declarations are handled explicitly as the "program header" in
            // generateCode().
            break;
        case ProgramElement::Kind::kStructDefinition:
            // All struct type declarations are handled explicitly as the "program header" in
            // generateCode().
            break;
        case ProgramElement::Kind::kFunctionPrototype:
            // A WGSL function declaration must contain its body and the function name is in scope
            // for the entire program (see https://www.w3.org/TR/WGSL/#function-declaration and
            // https://www.w3.org/TR/WGSL/#declaration-and-scope).
            //
            // As such, we don't emit function prototypes.
            break;
        case ProgramElement::Kind::kFunction:
            this->writeFunction(e.as<FunctionDefinition>());
            break;
        default:
            SkDEBUGFAILF("unsupported program element: %s\n", e.description().c_str());
            break;
    }
}

void WGSLCodeGenerator::writeStageInputStruct() {
    std::string structNamePrefix;
    if (fProgram.fConfig->fKind == ProgramKind::kVertex) {
        structNamePrefix = "VS";
    } else if (fProgram.fConfig->fKind == ProgramKind::kFragment) {
        structNamePrefix = "FS";
    } else {
        // There's no need to declare pipeline stage outputs.
        return;
    }

    this->write("struct ");
    this->write(structNamePrefix);
    this->writeLine("In {");
    fIndentation++;

    // TODO(skia:13092): Remember all variables that are added to the input struct here so they
    // can be referenced correctly when handling variable references.
    for (const ProgramElement* e : fProgram.elements()) {
        if (e->is<GlobalVarDeclaration>()) {
            const Variable& v =
                    e->as<GlobalVarDeclaration>().declaration()->as<VarDeclaration>().var();
            if (v.modifiers().fFlags & Modifiers::kIn_Flag) {
                this->writePipelineIODeclaration(v.modifiers(), v.type(), v.name());
            }
        } else if (e->is<InterfaceBlock>()) {
            const Variable& v = e->as<InterfaceBlock>().variable();
            // Merge all the members of `in` interface blocks to the input struct, which are
            // specified as either "builtin" or with a "layout(location=".
            //
            // TODO(armansito): Is it legal to have an interface block without a storage qualifier
            // but with members that have individual storage qualifiers?
            if (v.modifiers().fFlags & Modifiers::kIn_Flag) {
                for (const auto& f : v.type().fields()) {
                    this->writePipelineIODeclaration(f.fModifiers, *f.fType, f.fName);
                }
            }
        }
    }

    fIndentation--;
    this->writeLine("};");
}

void WGSLCodeGenerator::writeStageOutputStruct() {
    std::string structNamePrefix;
    if (fProgram.fConfig->fKind == ProgramKind::kVertex) {
        structNamePrefix = "VS";
    } else if (fProgram.fConfig->fKind == ProgramKind::kFragment) {
        structNamePrefix = "FS";
    } else {
        // There's no need to declare pipeline stage outputs.
        return;
    }

    this->write("struct ");
    this->write(structNamePrefix);
    this->writeLine("Out {");
    fIndentation++;

    // TODO(skia:13092): Remember all variables that are added to the output struct here so they
    // can be referenced correctly when handling variable references.
    for (const ProgramElement* e : fProgram.elements()) {
        if (e->is<GlobalVarDeclaration>()) {
            const Variable& v =
                    e->as<GlobalVarDeclaration>().declaration()->as<VarDeclaration>().var();
            if (v.modifiers().fFlags & Modifiers::kOut_Flag) {
                this->writePipelineIODeclaration(v.modifiers(), v.type(), v.name());
            }
        } else if (e->is<InterfaceBlock>()) {
            const Variable& v = e->as<InterfaceBlock>().variable();
            // Merge all the members of `out` interface blocks to the output struct, which are
            // specified as either "builtin" or with a "layout(location=".
            //
            // TODO(armansito): Is it legal to have an interface block without a storage qualifier
            // but with members that have individual storage qualifiers?
            if (v.modifiers().fFlags & Modifiers::kOut_Flag) {
                for (const auto& f : v.type().fields()) {
                    this->writePipelineIODeclaration(f.fModifiers, *f.fType, f.fName);
                }
            }
        }
    }

    fIndentation--;
    this->writeLine("};");
}

}  // namespace SkSL
