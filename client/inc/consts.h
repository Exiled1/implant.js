#pragma once

#define CONST_MEM_RW 0x1
#define CONST_MEM_RWX 0x2

#define CONST_MODE_R 0x1
#define CONST_MODE_W 0x2
#define CONST_MODE_RW 0x4

#define CONST_SEEK_SET 0x1
#define CONST_SEEK_END 0x2
#define CONST_SEEK_CUR 0x3

#define CONST_TYPE_VOID 0x1
#define CONST_TYPE_INTEGER 0x2
#define CONST_TYPE_POINTER 0x3
#define CONST_TYPE_BOOL 0x4
#define CONST_TYPE_STRING 0x5

#define CONST_OS_LINUX 0xc1
#define CONST_OS_WINDOWS 0xc2

#define MODULE_NAME "module.js"

#define CONTEXT_GROUP_ID 1

#define STATUS_SUCCESS 0xa0
#define STATUS_FAILURE 0xa1
#define STATUS_TERM 0xa2
#define STATUS_RUNNING 0xa3