#include "flist.capnp.h"
/* AUTO GENERATED - DO NOT EDIT */
#ifdef __GNUC__
# define capnp_unused __attribute__((unused))
# define capnp_use(x) (void) x;
#else
# define capnp_unused
# define capnp_use(x)
#endif

static const capn_text capn_val0 = {0,"",0};

FileBlock_ptr new_FileBlock(struct capn_segment *s) {
	FileBlock_ptr p;
	p.p = capn_new_struct(s, 0, 2);
	return p;
}
FileBlock_list new_FileBlock_list(struct capn_segment *s, int len) {
	FileBlock_list p;
	p.p = capn_new_list(s, len, 0, 2);
	return p;
}
void read_FileBlock(struct FileBlock *s capnp_unused, FileBlock_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->hash = capn_get_data(p.p, 0);
	s->key = capn_get_data(p.p, 1);
}
void write_FileBlock(const struct FileBlock *s capnp_unused, FileBlock_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_setp(p.p, 0, s->hash.p);
	capn_setp(p.p, 1, s->key.p);
}
void get_FileBlock(struct FileBlock *s, FileBlock_list l, int i) {
	FileBlock_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_FileBlock(s, p);
}
void set_FileBlock(const struct FileBlock *s, FileBlock_list l, int i) {
	FileBlock_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_FileBlock(s, p);
}

File_ptr new_File(struct capn_segment *s) {
	File_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
File_list new_File_list(struct capn_segment *s, int len) {
	File_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_File(struct File *s capnp_unused, File_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->blockSize = capn_read16(p.p, 0);
	s->blocks.p = capn_getp(p.p, 0, 0);
}
void write_File(const struct File *s capnp_unused, File_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, s->blockSize);
	capn_setp(p.p, 0, s->blocks.p);
}
void get_File(struct File *s, File_list l, int i) {
	File_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_File(s, p);
}
void set_File(const struct File *s, File_list l, int i) {
	File_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_File(s, p);
}

Link_ptr new_Link(struct capn_segment *s) {
	Link_ptr p;
	p.p = capn_new_struct(s, 0, 1);
	return p;
}
Link_list new_Link_list(struct capn_segment *s, int len) {
	Link_list p;
	p.p = capn_new_list(s, len, 0, 1);
	return p;
}
void read_Link(struct Link *s capnp_unused, Link_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->target = capn_get_text(p.p, 0, capn_val0);
}
void write_Link(const struct Link *s capnp_unused, Link_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->target);
}
void get_Link(struct Link *s, Link_list l, int i) {
	Link_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_Link(s, p);
}
void set_Link(const struct Link *s, Link_list l, int i) {
	Link_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_Link(s, p);
}

Special_ptr new_Special(struct capn_segment *s) {
	Special_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
Special_list new_Special_list(struct capn_segment *s, int len) {
	Special_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_Special(struct Special *s capnp_unused, Special_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->type = (enum Special_Type)(int) capn_read16(p.p, 0);
	s->data = capn_get_data(p.p, 0);
}
void write_Special(const struct Special *s capnp_unused, Special_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, (uint16_t) (s->type));
	capn_setp(p.p, 0, s->data.p);
}
void get_Special(struct Special *s, Special_list l, int i) {
	Special_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_Special(s, p);
}
void set_Special(const struct Special *s, Special_list l, int i) {
	Special_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_Special(s, p);
}

SubDir_ptr new_SubDir(struct capn_segment *s) {
	SubDir_ptr p;
	p.p = capn_new_struct(s, 0, 1);
	return p;
}
SubDir_list new_SubDir_list(struct capn_segment *s, int len) {
	SubDir_list p;
	p.p = capn_new_list(s, len, 0, 1);
	return p;
}
void read_SubDir(struct SubDir *s capnp_unused, SubDir_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->key = capn_get_text(p.p, 0, capn_val0);
}
void write_SubDir(const struct SubDir *s capnp_unused, SubDir_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->key);
}
void get_SubDir(struct SubDir *s, SubDir_list l, int i) {
	SubDir_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_SubDir(s, p);
}
void set_SubDir(const struct SubDir *s, SubDir_list l, int i) {
	SubDir_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_SubDir(s, p);
}

Inode_ptr new_Inode(struct capn_segment *s) {
	Inode_ptr p;
	p.p = capn_new_struct(s, 24, 3);
	return p;
}
Inode_list new_Inode_list(struct capn_segment *s, int len) {
	Inode_list p;
	p.p = capn_new_list(s, len, 24, 3);
	return p;
}
void read_Inode(struct Inode *s capnp_unused, Inode_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->name = capn_get_text(p.p, 0, capn_val0);
	s->size = capn_read64(p.p, 0);
	s->attributes_which = (enum Inode_attributes_which)(int) capn_read16(p.p, 8);
	switch (s->attributes_which) {
	case Inode_attributes_dir:
	case Inode_attributes_file:
	case Inode_attributes_link:
	case Inode_attributes_special:
		s->attributes.special.p = capn_getp(p.p, 1, 0);
		break;
	default:
		break;
	}
	s->aclkey = capn_get_text(p.p, 2, capn_val0);
	s->modificationTime = capn_read32(p.p, 12);
	s->creationTime = capn_read32(p.p, 16);
}
void write_Inode(const struct Inode *s capnp_unused, Inode_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->name);
	capn_write64(p.p, 0, s->size);
	capn_write16(p.p, 8, s->attributes_which);
	switch (s->attributes_which) {
	case Inode_attributes_dir:
	case Inode_attributes_file:
	case Inode_attributes_link:
	case Inode_attributes_special:
		capn_setp(p.p, 1, s->attributes.special.p);
		break;
	default:
		break;
	}
	capn_set_text(p.p, 2, s->aclkey);
	capn_write32(p.p, 12, s->modificationTime);
	capn_write32(p.p, 16, s->creationTime);
}
void get_Inode(struct Inode *s, Inode_list l, int i) {
	Inode_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_Inode(s, p);
}
void set_Inode(const struct Inode *s, Inode_list l, int i) {
	Inode_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_Inode(s, p);
}

Dir_ptr new_Dir(struct capn_segment *s) {
	Dir_ptr p;
	p.p = capn_new_struct(s, 16, 5);
	return p;
}
Dir_list new_Dir_list(struct capn_segment *s, int len) {
	Dir_list p;
	p.p = capn_new_list(s, len, 16, 5);
	return p;
}
void read_Dir(struct Dir *s capnp_unused, Dir_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->name = capn_get_text(p.p, 0, capn_val0);
	s->location = capn_get_text(p.p, 1, capn_val0);
	s->contents.p = capn_getp(p.p, 2, 0);
	s->parent = capn_get_text(p.p, 3, capn_val0);
	s->size = capn_read64(p.p, 0);
	s->aclkey = capn_get_text(p.p, 4, capn_val0);
	s->modificationTime = capn_read32(p.p, 8);
	s->creationTime = capn_read32(p.p, 12);
}
void write_Dir(const struct Dir *s capnp_unused, Dir_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->name);
	capn_set_text(p.p, 1, s->location);
	capn_setp(p.p, 2, s->contents.p);
	capn_set_text(p.p, 3, s->parent);
	capn_write64(p.p, 0, s->size);
	capn_set_text(p.p, 4, s->aclkey);
	capn_write32(p.p, 8, s->modificationTime);
	capn_write32(p.p, 12, s->creationTime);
}
void get_Dir(struct Dir *s, Dir_list l, int i) {
	Dir_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_Dir(s, p);
}
void set_Dir(const struct Dir *s, Dir_list l, int i) {
	Dir_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_Dir(s, p);
}

UserGroup_ptr new_UserGroup(struct capn_segment *s) {
	UserGroup_ptr p;
	p.p = capn_new_struct(s, 8, 2);
	return p;
}
UserGroup_list new_UserGroup_list(struct capn_segment *s, int len) {
	UserGroup_list p;
	p.p = capn_new_list(s, len, 8, 2);
	return p;
}
void read_UserGroup(struct UserGroup *s capnp_unused, UserGroup_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->name = capn_get_text(p.p, 0, capn_val0);
	s->iyoId = capn_get_text(p.p, 1, capn_val0);
	s->iyoInt = capn_read64(p.p, 0);
}
void write_UserGroup(const struct UserGroup *s capnp_unused, UserGroup_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->name);
	capn_set_text(p.p, 1, s->iyoId);
	capn_write64(p.p, 0, s->iyoInt);
}
void get_UserGroup(struct UserGroup *s, UserGroup_list l, int i) {
	UserGroup_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_UserGroup(s, p);
}
void set_UserGroup(const struct UserGroup *s, UserGroup_list l, int i) {
	UserGroup_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_UserGroup(s, p);
}

ACI_ptr new_ACI(struct capn_segment *s) {
	ACI_ptr p;
	p.p = capn_new_struct(s, 8, 3);
	return p;
}
ACI_list new_ACI_list(struct capn_segment *s, int len) {
	ACI_list p;
	p.p = capn_new_list(s, len, 8, 3);
	return p;
}
void read_ACI(struct ACI *s capnp_unused, ACI_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->uname = capn_get_text(p.p, 0, capn_val0);
	s->gname = capn_get_text(p.p, 1, capn_val0);
	s->mode = capn_read16(p.p, 0);
	s->rights.p = capn_getp(p.p, 2, 0);
	s->id = capn_read32(p.p, 4);
}
void write_ACI(const struct ACI *s capnp_unused, ACI_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->uname);
	capn_set_text(p.p, 1, s->gname);
	capn_write16(p.p, 0, s->mode);
	capn_setp(p.p, 2, s->rights.p);
	capn_write32(p.p, 4, s->id);
}
void get_ACI(struct ACI *s, ACI_list l, int i) {
	ACI_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ACI(s, p);
}
void set_ACI(const struct ACI *s, ACI_list l, int i) {
	ACI_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ACI(s, p);
}

ACI_Right_ptr new_ACI_Right(struct capn_segment *s) {
	ACI_Right_ptr p;
	p.p = capn_new_struct(s, 8, 1);
	return p;
}
ACI_Right_list new_ACI_Right_list(struct capn_segment *s, int len) {
	ACI_Right_list p;
	p.p = capn_new_list(s, len, 8, 1);
	return p;
}
void read_ACI_Right(struct ACI_Right *s capnp_unused, ACI_Right_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->right = capn_get_text(p.p, 0, capn_val0);
	s->usergroupid = capn_read16(p.p, 0);
}
void write_ACI_Right(const struct ACI_Right *s capnp_unused, ACI_Right_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->right);
	capn_write16(p.p, 0, s->usergroupid);
}
void get_ACI_Right(struct ACI_Right *s, ACI_Right_list l, int i) {
	ACI_Right_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ACI_Right(s, p);
}
void set_ACI_Right(const struct ACI_Right *s, ACI_Right_list l, int i) {
	ACI_Right_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ACI_Right(s, p);
}
