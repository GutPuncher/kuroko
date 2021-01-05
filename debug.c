#include <stdio.h>

#include "debug.h"
#include "vm.h"

void krk_disassembleChunk(KrkChunk * chunk, const char * name) {
	fprintf(stderr, "[%s from %s]\n", name, chunk->filename->chars);
	for (size_t offset = 0; offset < chunk->count;) {
		offset = krk_disassembleInstruction(chunk, offset);
	}
}

#define SIMPLE(opc) case opc: fprintf(stderr, "%s\n", #opc); return offset + 1;
#define CONSTANT(opc,more) case opc: { size_t constant = chunk->code[offset + 1]; \
	fprintf(stderr, "%-16s %4d ", #opc, (int)constant); \
	krk_printValueSafe(stderr, chunk->constants.values[constant]); \
	fprintf(stderr," (type=%s)\n", krk_typeName(chunk->constants.values[constant])); \
	more; \
	return offset + 2; } \
	case opc ## _LONG: { size_t constant = (chunk->code[offset + 1] << 16) | \
	(chunk->code[offset + 2] << 8) | (chunk->code[offset + 3]); \
	fprintf(stderr, "%-16s %4d ", #opc "_LONG", (int)constant); \
	krk_printValueSafe(stderr, chunk->constants.values[constant]); \
	fprintf(stderr," (type=%s)\n", krk_typeName(chunk->constants.values[constant])); \
	more; \
	return offset + 4; }
#define OPERANDB(opc) case opc: { uint32_t operand = chunk->code[offset + 1]; \
	fprintf(stderr, "%-16s %4d\n", #opc, (int)operand); \
	return offset + 2; }
#define OPERAND(opc) OPERANDB(opc) \
	case opc ## _LONG: { uint32_t operand = (chunk->code[offset + 1] << 16) | \
	(chunk->code[offset + 2] << 8) | (chunk->code[offset + 3]); \
	fprintf(stderr, "%-16s %4d\n", #opc "_LONG", (int)operand); \
	return offset + 4; }
#define JUMP(opc,sign) case opc: { uint16_t jump = (chunk->code[offset + 1] << 8) | \
	(chunk->code[offset + 2]); \
	fprintf(stderr, "%-16s %4d -> %d\n", #opc, (int)offset, (int)(offset + 3 sign jump)); \
	return offset + 3; }

#define CLOSURE_MORE \
	KrkFunction * function = AS_FUNCTION(chunk->constants.values[constant]); \
	for (size_t j = 0; j < function->upvalueCount; ++j) { \
		int isLocal = chunk->code[offset++ + 2]; \
		int index = chunk->code[offset++ + 2]; \
		fprintf(stderr, "%04d      |                     %s %d\n", \
			(int)offset - 2, isLocal ? "local" : "upvalue", index); \
	}

size_t krk_lineNumber(KrkChunk * chunk, size_t offset) {
	size_t line = 0;
	for (size_t i = 0; i < chunk->linesCount; ++i) {
		if (chunk->lines[i].startOffset > offset) break;
		line = chunk->lines[i].line;
	}
	return line;
}

size_t krk_disassembleInstruction(KrkChunk * chunk, size_t offset) {
	fprintf(stderr, "%04u ", (unsigned int)offset);
	if (offset > 0 && krk_lineNumber(chunk, offset) == krk_lineNumber(chunk, offset - 1)) {
		fprintf(stderr, "   | ");
	} else {
		fprintf(stderr, "%4d ", (int)krk_lineNumber(chunk, offset));
	}
	uint8_t opcode = chunk->code[offset];

	switch (opcode) {
		SIMPLE(OP_RETURN)
		SIMPLE(OP_ADD)
		SIMPLE(OP_SUBTRACT)
		SIMPLE(OP_MULTIPLY)
		SIMPLE(OP_DIVIDE)
		SIMPLE(OP_NEGATE)
		SIMPLE(OP_MODULO)
		SIMPLE(OP_NONE)
		SIMPLE(OP_TRUE)
		SIMPLE(OP_FALSE)
		SIMPLE(OP_NOT)
		SIMPLE(OP_EQUAL)
		SIMPLE(OP_GREATER)
		SIMPLE(OP_LESS)
		SIMPLE(OP_POP)
		SIMPLE(OP_INHERIT)
		SIMPLE(OP_RAISE)
		SIMPLE(OP_CLOSE_UPVALUE)
		SIMPLE(OP_DOCSTRING)
		SIMPLE(OP_CALL_STACK)
		SIMPLE(OP_BITOR)
		SIMPLE(OP_BITXOR)
		SIMPLE(OP_BITAND)
		SIMPLE(OP_SHIFTLEFT)
		SIMPLE(OP_SHIFTRIGHT)
		SIMPLE(OP_BITNEGATE)
		SIMPLE(OP_INVOKE_GETTER)
		SIMPLE(OP_INVOKE_SETTER)
		SIMPLE(OP_INVOKE_GETSLICE)
		SIMPLE(OP_SWAP)
		SIMPLE(OP_FINALIZE)
		OPERANDB(OP_DUP)
		OPERANDB(OP_EXPAND_ARGS)
		CONSTANT(OP_DEFINE_GLOBAL,(void)0)
		CONSTANT(OP_CONSTANT,(void)0)
		CONSTANT(OP_GET_GLOBAL,(void)0)
		CONSTANT(OP_SET_GLOBAL,(void)0)
		CONSTANT(OP_CLASS,(void)0)
		CONSTANT(OP_GET_PROPERTY, (void)0)
		CONSTANT(OP_SET_PROPERTY, (void)0)
		CONSTANT(OP_METHOD, (void)0)
		CONSTANT(OP_CLOSURE, CLOSURE_MORE)
		CONSTANT(OP_IMPORT, (void)0)
		CONSTANT(OP_GET_SUPER, (void)0)
		OPERAND(OP_KWARGS)
		OPERAND(OP_SET_LOCAL)
		OPERAND(OP_GET_LOCAL)
		OPERAND(OP_SET_UPVALUE)
		OPERAND(OP_GET_UPVALUE)
		OPERAND(OP_CALL)
		OPERAND(OP_INC)
		JUMP(OP_JUMP,+)
		JUMP(OP_JUMP_IF_FALSE,+)
		JUMP(OP_JUMP_IF_TRUE,+)
		JUMP(OP_LOOP,-)
		JUMP(OP_PUSH_TRY,+)
	}
	fprintf(stderr, "Unknown opcode: %02x\n", opcode);
	return offset + 1;
}

