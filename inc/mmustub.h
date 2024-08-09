#ifndef MMU_STUB_H_DEFINED
#define MMU_STUB_H_DEFINED


// A value of this type identifies a file.
// - For computer systems, it can be `char*`;
// - For embedded systems, this can be as tiny as `unsigned char`.
typedef char* stub_mmuFileID_t;

// This type of value describes the size of buffers, *in bytes*.
// You can use it as-is, unless you're porting it to something like Z80...
typedef unsigned int stub_mmuMemSize_t;

// This type of value is used to report status of MMU stub functions.
// Signed type is recommended, but it's all up to you. ;)
typedef int stub_mmuStatus_t;

#define STUB_MMU_OK ((stub_mmuStatus_t)0)
#define STUB_MMU_ERROR ((stub_mmuStatus_t)-1)

typedef struct {
	stub_mmuFileID_t codeMemoryID;
	stub_mmuFileID_t dataMemoryID;
	stub_mmuMemSize_t codeMemorySize;
	stub_mmuMemSize_t dataMemorySize;
} stub_mmuInitStruct_t;


/// @brief Load code memory.
/// 		You need to implement this function YOURSELF.
///		Hint: You can call this function in `stub_mmuInitCodeMemory` to reuse logic
/// @param s `codeMemoryID` refers to code memory file,
///		`codeMemorySize` specifies size in bytes.
/// @param p Points to code memory
/// @returns `STUB_MMU_OK` on success.
extern stub_mmuStatus_t stub_mmuLoadCodeMemory(const stub_mmuInitStruct_t s, const void *p);

/// @brief Load data memory.
/// 		You need to implement this function YOURSELF.
///		Hint: You can call this function in `stub_mmuInitDataMemory` to reuse logic
/// @param s `dataMemoryID` refers to data memory file,
///		`dataMemorySize` specifies size in bytes.
/// @param p A pointer to data memory.
/// @returns `STUB_MMU_OK` on success.
extern stub_mmuStatus_t stub_mmuLoadDataMemory(const stub_mmuInitStruct_t s, const void *p);

/// @brief Save data memory.
/// 		You need to implement this function YOURSELF.
/// @param s `dataMemoryID` refers to data memory file *to save*
///		`dataMemorySize` specifies size in bytes.
/// @param p A pointer to data memory.
/// @returns `STUB_MMU_OK` on success.
extern stub_mmuStatus_t stub_mmuSaveDataMemory(const stub_mmuInitStruct_t s, const void *p);

/// @brief Initialize code memory.
/// 		You need to implement this function YOURSELF.
/// @param s `codeMemoryID` refers to code memory file,
///		`codeMemorySize` specifies size in bytes.
/// @returns A pointer to code memory, `NULL` if failed.
extern void* stub_mmuInitCodeMemory(const stub_mmuInitStruct_t s);

/// @brief Initialize data memory.
/// 		You need to implement this function YOURSELF.
/// @param s `dataMemoryID` refers to data memory file,
///		`dataMemorySize` specifies size in bytes.
/// @returns A pointer to data memory, `NULL` if failed.
extern void* stub_mmuInitDataMemory(const stub_mmuInitStruct_t s);

/// @brief Free/Release code memory.
/// 		You need to implement this function YOURSELF.
/// @param p A pointer to code memory.
extern void stub_mmuFreeCodeMemory(const void *p);

/// @brief Free/Release data memory.
/// 		You need to implement this function YOURSELF.
/// @param p A pointer to data memory.
extern void stub_mmuFreeDataMemory(const void *p);


#endif
