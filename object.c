#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJECT(type, objectType) \
	(type*)allocateObject(sizeof(type), objectType)

static KrkObj * allocateObject(size_t size, ObjType type) {
	KrkObj * object = (KrkObj*)krk_reallocate(NULL, 0, size);
	object->type = type;
	object->isMarked = 0;
	object->next = vm.objects;
	vm.objects = object;
	return object;
}

static KrkString * allocateString(char * chars, size_t length, uint32_t hash) {
	KrkString * string = ALLOCATE_OBJECT(KrkString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	krk_push(OBJECT_VAL(string));
	krk_tableSet(&vm.strings, OBJECT_VAL(string), NONE_VAL());
	krk_pop();
	return string;
}

static uint32_t hashString(const char * key, size_t length) {
	uint32_t hash = 0;
	/* This is the so-called "sdbm" hash. It comes from a piece of
	 * public domain code from a clone of ndbm. */
	for (size_t i = 0; i < length; ++i) {
		hash = (int)key[i] + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

KrkString * krk_takeString(char * chars, size_t length) {
	uint32_t hash = hashString(chars, length);
	KrkString * interned = krk_tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}
	return allocateString(chars, length, hash);
}

KrkString * krk_copyString(const char * chars, size_t length) {
	uint32_t hash = hashString(chars, length);
	KrkString * interned = krk_tableFindString(&vm.strings, chars, length, hash);
	if (interned) return interned;
	char * heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

#define NAME(obj) ((obj)->name ? obj->name->chars : "(unnamed)")
void krk_printObject(FILE * f, KrkValue value) {
	switch (OBJECT_TYPE(value)) {
		case OBJ_STRING:
			fprintf(f,"\"");
			for (char * c = AS_CSTRING(value); *c; ++c) {
				switch (*c) {
					/* XXX: Other non-printables should probably be escaped as well. */
					case '\n': fprintf(f,"\\n"); break;
					case '\r': fprintf(f,"\\r"); break;
					case '\t': fprintf(f,"\\t"); break;
					case '"': fprintf(f,"\\\""); break;
					case '\033': fprintf(f,"\\["); break;
					default: fprintf(f,"%c",*c); break;
				}
			}
			fprintf(f,"\"");
			break;
		case OBJ_FUNCTION:
			if (AS_FUNCTION(value)->name == NULL) fprintf(f, "<module>");
			else fprintf(f, "<def %s>", NAME(AS_FUNCTION(value)));
			break;
		case OBJ_NATIVE:
			fprintf(f, "<native bind>");
			break;
		case OBJ_CLOSURE:
			fprintf(f, "<closure <def %s>>", NAME(AS_CLOSURE(value)->function));
			break;
		case OBJ_UPVALUE:
			fprintf(f, "<upvalue>");
			break;
		case OBJ_CLASS:
			fprintf(f, "<class %s>", NAME(AS_CLASS(value)));
			break;
		case OBJ_INSTANCE:
			fprintf(f, "<instance of %s>", NAME(AS_INSTANCE(value)->_class));
			break;
		case OBJ_BOUND_METHOD:
			fprintf(f, "<bound <def %s>>", (AS_BOUND_METHOD(value)->method->type == OBJ_CLOSURE) ?
				NAME(((KrkClosure*)AS_BOUND_METHOD(value)->method)->function) : (
					(AS_BOUND_METHOD(value)->method->type == OBJ_NATIVE) ? "<native>" : "<unknown>"
				));
			break;
	}
}

KrkFunction * krk_newFunction() {
	KrkFunction * function = ALLOCATE_OBJECT(KrkFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	krk_initChunk(&function->chunk);
	return function;
}

KrkNative * krk_newNative(NativeFn function) {
	KrkNative * native = ALLOCATE_OBJECT(KrkNative, OBJ_NATIVE);
	native->function = function;
	native->isMethod = 0;
	return native;
}

KrkClosure * krk_newClosure(KrkFunction * function) {
	KrkUpvalue ** upvalues = ALLOCATE(KrkUpvalue*, function->upvalueCount);
	for (size_t i = 0; i < function->upvalueCount; ++i) {
		upvalues[i] = NULL;
	}
	KrkClosure * closure = ALLOCATE_OBJECT(KrkClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

KrkUpvalue * krk_newUpvalue(int slot) {
	KrkUpvalue * upvalue = ALLOCATE_OBJECT(KrkUpvalue, OBJ_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NONE_VAL();
	return upvalue;
}

KrkClass * krk_newClass(KrkString * name) {
	KrkClass * _class = ALLOCATE_OBJECT(KrkClass, OBJ_CLASS);
	_class->name = name;
	_class->filename = NULL;
	krk_initTable(&_class->methods);
	return _class;
}

KrkInstance * krk_newInstance(KrkClass * _class) {
	KrkInstance * instance = ALLOCATE_OBJECT(KrkInstance, OBJ_INSTANCE);
	instance->_class = _class;
	krk_initTable(&instance->fields);
	return instance;
}

KrkBoundMethod * krk_newBoundMethod(KrkValue receiver, KrkObj * method) {
	KrkBoundMethod * bound = ALLOCATE_OBJECT(KrkBoundMethod, OBJ_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}
