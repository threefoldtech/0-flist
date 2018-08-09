#ifndef CAPN_AE9223E76351538A
#define CAPN_AE9223E76351538A
/* AUTO GENERATED - DO NOT EDIT */
#include <unistd.h>
#include <capnp_c.h>

#if CAPN_VERSION != 1
#error "version mismatch between capnp_c.h and generated code"
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct FileBlock;
struct File;
struct Link;
struct Special;
struct SubDir;
struct Inode;
struct Dir;
struct UserGroup;
struct ACI;
struct ACI_Right;

typedef struct {capn_ptr p;} FileBlock_ptr;
typedef struct {capn_ptr p;} File_ptr;
typedef struct {capn_ptr p;} Link_ptr;
typedef struct {capn_ptr p;} Special_ptr;
typedef struct {capn_ptr p;} SubDir_ptr;
typedef struct {capn_ptr p;} Inode_ptr;
typedef struct {capn_ptr p;} Dir_ptr;
typedef struct {capn_ptr p;} UserGroup_ptr;
typedef struct {capn_ptr p;} ACI_ptr;
typedef struct {capn_ptr p;} ACI_Right_ptr;

typedef struct {capn_ptr p;} FileBlock_list;
typedef struct {capn_ptr p;} File_list;
typedef struct {capn_ptr p;} Link_list;
typedef struct {capn_ptr p;} Special_list;
typedef struct {capn_ptr p;} SubDir_list;
typedef struct {capn_ptr p;} Inode_list;
typedef struct {capn_ptr p;} Dir_list;
typedef struct {capn_ptr p;} UserGroup_list;
typedef struct {capn_ptr p;} ACI_list;
typedef struct {capn_ptr p;} ACI_Right_list;

enum Special_Type {
	Special_Type_socket = 0,
	Special_Type_block = 1,
	Special_Type_chardev = 2,
	Special_Type_fifopipe = 3,
	Special_Type_unknown = 4
};

struct FileBlock {
	capn_data hash;
	capn_data key;
};

static const size_t FileBlock_word_count = 0;

static const size_t FileBlock_pointer_count = 2;

static const size_t FileBlock_struct_bytes_count = 16;

struct File {
	uint16_t blockSize;
	FileBlock_list blocks;
};

static const size_t File_word_count = 1;

static const size_t File_pointer_count = 1;

static const size_t File_struct_bytes_count = 16;

struct Link {
	capn_text target;
};

static const size_t Link_word_count = 0;

static const size_t Link_pointer_count = 1;

static const size_t Link_struct_bytes_count = 8;

struct Special {
	enum Special_Type type;
	capn_data data;
};

static const size_t Special_word_count = 1;

static const size_t Special_pointer_count = 1;

static const size_t Special_struct_bytes_count = 16;

struct SubDir {
	capn_text key;
};

static const size_t SubDir_word_count = 0;

static const size_t SubDir_pointer_count = 1;

static const size_t SubDir_struct_bytes_count = 8;
enum Inode_attributes_which {
	Inode_attributes_dir = 0,
	Inode_attributes_file = 1,
	Inode_attributes_link = 2,
	Inode_attributes_special = 3
};

struct Inode {
	capn_text name;
	uint64_t size;
	enum Inode_attributes_which attributes_which;
	union {
		SubDir_ptr dir;
		File_ptr file;
		Link_ptr link;
		Special_ptr special;
	} attributes;
	capn_text aclkey;
	uint32_t modificationTime;
	uint32_t creationTime;
};

static const size_t Inode_word_count = 3;

static const size_t Inode_pointer_count = 3;

static const size_t Inode_struct_bytes_count = 48;

struct Dir {
	capn_text name;
	capn_text location;
	Inode_list contents;
	capn_text parent;
	uint64_t size;
	capn_text aclkey;
	uint32_t modificationTime;
	uint32_t creationTime;
};

static const size_t Dir_word_count = 2;

static const size_t Dir_pointer_count = 5;

static const size_t Dir_struct_bytes_count = 56;

struct UserGroup {
	capn_text name;
	capn_text iyoId;
	uint64_t iyoInt;
};

static const size_t UserGroup_word_count = 1;

static const size_t UserGroup_pointer_count = 2;

static const size_t UserGroup_struct_bytes_count = 24;

struct ACI {
	capn_text uname;
	capn_text gname;
	uint16_t mode;
	ACI_Right_list rights;
	uint32_t id;
};

static const size_t ACI_word_count = 1;

static const size_t ACI_pointer_count = 3;

static const size_t ACI_struct_bytes_count = 32;

struct ACI_Right {
	capn_text right;
	uint16_t usergroupid;
};

static const size_t ACI_Right_word_count = 1;

static const size_t ACI_Right_pointer_count = 1;

static const size_t ACI_Right_struct_bytes_count = 16;

FileBlock_ptr new_FileBlock(struct capn_segment*);
File_ptr new_File(struct capn_segment*);
Link_ptr new_Link(struct capn_segment*);
Special_ptr new_Special(struct capn_segment*);
SubDir_ptr new_SubDir(struct capn_segment*);
Inode_ptr new_Inode(struct capn_segment*);
Dir_ptr new_Dir(struct capn_segment*);
UserGroup_ptr new_UserGroup(struct capn_segment*);
ACI_ptr new_ACI(struct capn_segment*);
ACI_Right_ptr new_ACI_Right(struct capn_segment*);

FileBlock_list new_FileBlock_list(struct capn_segment*, int len);
File_list new_File_list(struct capn_segment*, int len);
Link_list new_Link_list(struct capn_segment*, int len);
Special_list new_Special_list(struct capn_segment*, int len);
SubDir_list new_SubDir_list(struct capn_segment*, int len);
Inode_list new_Inode_list(struct capn_segment*, int len);
Dir_list new_Dir_list(struct capn_segment*, int len);
UserGroup_list new_UserGroup_list(struct capn_segment*, int len);
ACI_list new_ACI_list(struct capn_segment*, int len);
ACI_Right_list new_ACI_Right_list(struct capn_segment*, int len);

void read_FileBlock(struct FileBlock*, FileBlock_ptr);
void read_File(struct File*, File_ptr);
void read_Link(struct Link*, Link_ptr);
void read_Special(struct Special*, Special_ptr);
void read_SubDir(struct SubDir*, SubDir_ptr);
void read_Inode(struct Inode*, Inode_ptr);
void read_Dir(struct Dir*, Dir_ptr);
void read_UserGroup(struct UserGroup*, UserGroup_ptr);
void read_ACI(struct ACI*, ACI_ptr);
void read_ACI_Right(struct ACI_Right*, ACI_Right_ptr);

void write_FileBlock(const struct FileBlock*, FileBlock_ptr);
void write_File(const struct File*, File_ptr);
void write_Link(const struct Link*, Link_ptr);
void write_Special(const struct Special*, Special_ptr);
void write_SubDir(const struct SubDir*, SubDir_ptr);
void write_Inode(const struct Inode*, Inode_ptr);
void write_Dir(const struct Dir*, Dir_ptr);
void write_UserGroup(const struct UserGroup*, UserGroup_ptr);
void write_ACI(const struct ACI*, ACI_ptr);
void write_ACI_Right(const struct ACI_Right*, ACI_Right_ptr);

void get_FileBlock(struct FileBlock*, FileBlock_list, int i);
void get_File(struct File*, File_list, int i);
void get_Link(struct Link*, Link_list, int i);
void get_Special(struct Special*, Special_list, int i);
void get_SubDir(struct SubDir*, SubDir_list, int i);
void get_Inode(struct Inode*, Inode_list, int i);
void get_Dir(struct Dir*, Dir_list, int i);
void get_UserGroup(struct UserGroup*, UserGroup_list, int i);
void get_ACI(struct ACI*, ACI_list, int i);
void get_ACI_Right(struct ACI_Right*, ACI_Right_list, int i);

void set_FileBlock(const struct FileBlock*, FileBlock_list, int i);
void set_File(const struct File*, File_list, int i);
void set_Link(const struct Link*, Link_list, int i);
void set_Special(const struct Special*, Special_list, int i);
void set_SubDir(const struct SubDir*, SubDir_list, int i);
void set_Inode(const struct Inode*, Inode_list, int i);
void set_Dir(const struct Dir*, Dir_list, int i);
void set_UserGroup(const struct UserGroup*, UserGroup_list, int i);
void set_ACI(const struct ACI*, ACI_list, int i);
void set_ACI_Right(const struct ACI_Right*, ACI_Right_list, int i);

#ifdef __cplusplus
}
#endif
#endif
