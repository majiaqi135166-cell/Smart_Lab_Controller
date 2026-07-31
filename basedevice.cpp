#include "basedevice.h"

// 析构函数实现（确保 vtable 生成）
// 虽然头文件里有内联实现，但在某些编译器下显式定义更稳健
// BaseDevice::~BaseDevice() { }
