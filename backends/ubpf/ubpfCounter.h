#ifndef P4C_UBPFCOUNTER_H
#define P4C_UBPFCOUNTER_H

#include "ubpfTable.h"

namespace UBPF {

    enum CounterType {
        PACKETS,
        BYTES,
        PACKETS_AND_BYTES
    };

    class UBPFCounter final : public UBPFTableBase {
        size_t    size;
        bool      isHash;
    public:
        CounterType type;

        UBPFCounter(const UBPFProgram *program, const IR::ExternBlock *block,
                    cstring name, EBPF::CodeGenInspector *codeGen);
        void emitCounterDataType(EBPF::CodeBuilder *builder);
        void emitInstance(EBPF::CodeBuilder *builder);
        void emitMethodInvocation(EBPF::CodeBuilder *builder,
                                  const P4::ExternMethod *method);

    protected:
        void emitCount(EBPF::CodeBuilder *builder,
                       const IR::MethodCallExpression *expression);
    };
}

#endif //P4C_UBPFCOUNTER_H
