#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "error.h"
#include "hashmap.h"
#include "object.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void terminateVM();

static InterpreterResult run();

static Value makeStrTemplate();

static int currentOpLine();
static void resetStack();
static void push(Value value);
static Value pop();
static Value peek(int depth);

VM vm;
Chunk compilingChunk;

InterpreterResult interpret(const char *source) {
    initChunk(&compilingChunk);
    if (compile(source, &compilingChunk)) {
        freeChunk(&compilingChunk);
        return INTERPRET_COMPILE_ERROR;
    }
    vm.ip = compilingChunk.codes;
    InterpreterResult res = run();
    freeChunk(&compilingChunk);
    return res;
    // terminateCompiler();
    // terminateVM();
}

void initVM(bool isREPL) {
    resetStack();
    initMap(&vm.stringLiterals);
    initMap(&vm.globals);
    vm.objs = NULL;
    vm.isREPL = isREPL;
}

static void terminateVM() {
    freeObjs(vm.objs);
    freeMap(&vm.stringLiterals);
    freeMap(&vm.globals);
}

static InterpreterResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (getConstantAtIndex(&compilingChunk, READ_BYTE()))
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define ARITHEMETIC_BINARY_OP(valueType, op)                                   \
    do {                                                                       \
        double b = AS_NUMBER(pop());                                           \
        double a = AS_NUMBER(pop());                                           \
        push(valueType(a op b));                                               \
    } while (false)

    for (;;) {
        disassembleInstruction(&compilingChunk, vm.ip - compilingChunk.codes);
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            mapPut(&vm.globals, name, peek(0));
            pop();
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString *name = READ_STRING();
            Value val;
            if (!mapGet(&vm.globals, name, &val)) {
                runtimeError(currentOpLine(), "Undefined Variable '%.*s'",
                             name->length, name->str);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(val);
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString *name = READ_STRING();
            if (mapPut(&vm.globals, name, peek(0))) {
                mapDelete(&vm.globals, name);
                runtimeError(currentOpLine(), "Undefined Variable '%.*s'",
                             name->length, name->str);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_PRINT: {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(a == b));
            break;
        }
        case OP_NOT_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(a != b));
            break;
        }
        case OP_LESS: {
            ARITHEMETIC_BINARY_OP(BOOL_VAL, <);
            break;
        }
        case OP_LESS_EQUAL: {
            ARITHEMETIC_BINARY_OP(BOOL_VAL, <=);
            break;
        }
        case OP_GREATER: {
            ARITHEMETIC_BINARY_OP(BOOL_VAL, >);
            break;
        }
        case OP_GREATER_EQUAL: {
            ARITHEMETIC_BINARY_OP(BOOL_VAL, >=);
            break;
        }
        case OP_ADD: {
            ARITHEMETIC_BINARY_OP(NUMBER_VAL, +);
            break;
        }
        case OP_SUBTRACT: {
            ARITHEMETIC_BINARY_OP(NUMBER_VAL, -);
            break;
        }
        case OP_DIVIDE: {
            ARITHEMETIC_BINARY_OP(NUMBER_VAL, /);
            break;
        }
        case OP_MULTIPLY: {
            ARITHEMETIC_BINARY_OP(NUMBER_VAL, *);
            break;
        }
        case OP_TEMPLATE: {
            push(makeStrTemplate());
            break;
        }
        case OP_NEGATE: {
            Value val = pop();
            if (!IS_NUMBER(val)) {
                // TODO: ADD RUNTIME ERROR
            }
            push(NUMBER_VAL(-AS_NUMBER(val)));
            break;
        }
        case OP_NOT: {
            Value val = pop();
            push(val != TRUE_VAL ? TRUE_VAL : FALSE_VAL);
            break;
        }
        case OP_CONSTANT: {
            Value val = READ_CONSTANT();
            push(val);
            break;
        }
        case OP_NIL:
            push(NIL_VAL);
            break;
        case OP_TRUE:
            push(TRUE_VAL);
            break;
        case OP_FALSE:
            push(FALSE_VAL);
            break;
        case OP_RETURN:
            goto end;
            break;
        case OP_POP:
            pop();
            break;
        }
    }
end:
    return INTERPRET_OK;
#undef ARITHEMETIC_BINARY_OP
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_BYTE
}

static int currentOpLine() {
    size_t instruction = vm.ip - compilingChunk.codes - 1;
    return compilingChunk.lines[instruction];
}

static Value makeStrTemplate() {
    FILE *stream;
    char *buf;
    size_t len;
    stream = open_memstream(&buf, &len);
    if (stream == NULL) {
        fprintf(stderr, "DOJO ERROR: Running out of memory\n");
        exit(1);
    }
    while (vm.count > 0) {
        printValueToFile(stream, pop());
    }
    fflush(stream);
    fclose(stream);

    ObjString *str = newObjString(buf, len);
    markUsingHeap(str);
    return OBJ_VAL(str);
}

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.count = 0;
}

static void push(Value value) {
    *(vm.stackTop++) = value;
    vm.count++;
}

static Value pop() {
    vm.count--;
    return *(--vm.stackTop);
}

static Value peek(int depth) {
    return *(vm.stackTop - depth - 1);
}
