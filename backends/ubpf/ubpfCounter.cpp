#include "ubpfCounter.h"

namespace UBPF {

    UBPFCounter::UBPFCounter(const UBPFProgram *program, const IR::ExternBlock *block, cstring name,
                             EBPF::CodeGenInspector *codeGen) :
            UBPFTableBase(program, name, codeGen) {
        auto typeParam = block->getParameterValue(program->model.counterModel.type.name);
        if (typeParam == nullptr) {
            ::error(ErrorType::ERR_INVALID,
                    "%1% (%2%): expected type argument; is the model corrupted?",
                    program->model.counterModel.type, name);
            return;
        }

        auto counterType = typeParam->to<IR::CompileTimeValue>();
        if (counterType->toString() == "PACKETS") {
            type = PACKETS;
        } else if (counterType->toString() == "BYTES") {
            type = BYTES;
        } else if (counterType->toString() == "PACKETS_AND_BYTES") {
            type = PACKETS_AND_BYTES;
        }

        auto di = block->node->to<IR::Declaration_Instance>();
        auto tp = program->typeMap->getType(di, true)->to<IR::Type_SpecializedCanonical>();

        keyType = tp->arguments->operator[](1);

        if (keyType == nullptr || !keyType->is<IR::Type_Bits>()) {
            ::error(ErrorType::ERR_UNSUPPORTED_ON_TARGET,
                    "%1% (%2%): Only bit<> type is supported on the target",
                    program->model.counterModel.index, name);
            return;
        }

        auto ubpfType = UBPFTypeFactory::instance->create(keyType);
        auto ubpfScalarType = ubpfType->to<UBPFScalarType>();
        keyTypeName = ubpfScalarType->getAsString();

        if (type != PACKETS_AND_BYTES) {
            valueType = new IR::Type_Bits(64, false);
            auto ubpfType = UBPFTypeFactory::instance->create(valueType);
            auto ubpfScalarType = ubpfType->to<UBPFScalarType>();
            valueTypeName = ubpfScalarType->getAsString();
        } else {
            valueTypeName = "counter_data";
        }

        auto sz = block->getParameterValue(program->model.counterModel.n_counters.name);
        if (sz == nullptr || !sz->is<IR::Constant>()) {
            ::error(ErrorType::ERR_INVALID,
                    "%1% (%2%): expected an integer argument; is the model corrupted?",
                    program->model.counterModel.n_counters, name);
            return;
        }

        auto cst = sz->to<IR::Constant>();
        if (!cst->fitsUint64()) {
            ::error(ErrorType::ERR_OVERLIMIT, "%1%: size too large", cst);
            return;
        }

        size = cst->asInt();
        if (size <= 0) {
            ::error(ErrorType::ERR_OVERLIMIT, "%1%: negative size", cst);
            return;
        }

        isHash = false;
    }

    void UBPFCounter::emitMethodInvocation(EBPF::CodeBuilder *builder, const P4::ExternMethod *method) {
        if (method->method->name.name ==
                program->model.counterModel.count.name) {
            emitCount(builder, method->expr);
            return;
        }
        error("%1%: Unexpected method for %2%", method->expr,
              program->model.counterModel.name);
    }

    void UBPFCounter::emitCount(EBPF::CodeBuilder *builder,
                                const IR::MethodCallExpression *expression) {
        cstring keyName = program->refMap->newName("counter_key");
        cstring currentValueName = program->refMap->newName("curr_val");

        if (type == PACKETS_AND_BYTES) {
            builder->appendFormat("struct %s", valueTypeName);
        } else {
            builder->appendFormat("%s", valueTypeName);
        }

        builder->spc();
        builder->append("*");
        builder->append(currentValueName);
        builder->endOfStatement(true);

        builder->emitIndent();
        builder->append(keyTypeName);
        builder->spc();
        builder->append(keyName);
        builder->append(" = ");
        auto index = expression->arguments->at(0);

        codeGen->visit(index->expression);
        builder->endOfStatement(true);

        builder->emitIndent();
        builder->append(currentValueName);
        builder->append(" = ");
        builder->target->emitTableLookup(builder, dataMapName, keyName, currentValueName);
        builder->endOfStatement(true);

        builder->emitIndent();
        builder->appendFormat("if (%s != NULL)", currentValueName.c_str());
        builder->spc();
        builder->blockStart();
        builder->emitIndent();

        if (type == PACKETS) {
            builder->appendFormat("*%s += 1;", currentValueName.c_str());
            builder->newline();
        } else if (type == BYTES) {
            builder->appendFormat("*%s += ", currentValueName.c_str());
            builder->append(program->lengthVar);
            builder->endOfStatement(true);
        } else if (type == PACKETS_AND_BYTES) {
            builder->appendFormat("%s->packets += 1;", currentValueName.c_str());
            builder->newline();
            builder->emitIndent();
            builder->appendFormat("%s->bytes += ", currentValueName.c_str());
            builder->append(program->lengthVar);
            builder->endOfStatement(true);
        } else {
            BUG("Type of counter was not initialized");
        }

        builder->blockEnd(false);
        builder->spc();
        builder->append("else");
        builder->spc();
        builder->blockStart();
        builder->emitIndent();
        if (type == PACKETS_AND_BYTES) {
            builder->appendFormat("struct %s", valueTypeName);
            builder->spc();
            builder->appendLine("init_val = {0};");
        } else {
            builder->append(valueTypeName);
            builder->spc();
            builder->appendLine("init_val = 0;");
        }
        builder->emitIndent();
        builder->target->emitTableUpdate(builder, dataMapName, keyName, "&init_val");
        builder->endOfStatement(true);
        builder->blockEnd(true);
    }

    void UBPFCounter::emitInstance(EBPF::CodeBuilder *builder) {
        EBPF::TableKind kind = isHash ? EBPF::TableHash : EBPF::TableArray;

        builder->target->emitTableDecl(
                builder, dataMapName, kind,
                keyTypeName,
                type == PACKETS_AND_BYTES ? "struct " + valueTypeName : valueTypeName,
                size);
    }

    void UBPFCounter::emitCounterDataType(EBPF::CodeBuilder *builder) {
        if (type == PACKETS_AND_BYTES) {
            builder->emitIndent();
            builder->appendFormat("struct %s ", valueTypeName.c_str());
            builder->blockStart();
            builder->emitIndent();
            builder->appendLine("uint64_t packets;");
            builder->emitIndent();
            builder->appendLine("uint64_t bytes;");
            builder->blockEnd(false);
            builder->endOfStatement(true);
        }
    }
}
