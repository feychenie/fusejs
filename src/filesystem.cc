#include "filesystem.h"
#include "reply.h"
#include "file_info.h"
#include "bindings.h"
#include "node_buffer.h"
#include "event.h"

namespace NodeFuse {

	static struct fuse_lowlevel_ops fuse_ops = {};

	// Symbols for FUSE operations
	static Persistent<String> init_sym        = NODE_PSYMBOL("init");
	static Persistent<String> destroy_sym     = NODE_PSYMBOL("destroy");
	static Persistent<String> lookup_sym      = NODE_PSYMBOL("lookup");
	static Persistent<String> forget_sym      = NODE_PSYMBOL("forget");
	static Persistent<String> getattr_sym     = NODE_PSYMBOL("getattr");
	static Persistent<String> setattr_sym     = NODE_PSYMBOL("setattr");
	static Persistent<String> readlink_sym    = NODE_PSYMBOL("readlink");
	static Persistent<String> mknod_sym       = NODE_PSYMBOL("mknod");
	static Persistent<String> mkdir_sym       = NODE_PSYMBOL("mkdir");
	static Persistent<String> unlink_sym      = NODE_PSYMBOL("unlink");
	static Persistent<String> rmdir_sym       = NODE_PSYMBOL("rmdir");
	static Persistent<String> symlink_sym     = NODE_PSYMBOL("symlink");
	static Persistent<String> rename_sym      = NODE_PSYMBOL("rename");
	static Persistent<String> link_sym        = NODE_PSYMBOL("link");
	static Persistent<String> open_sym        = NODE_PSYMBOL("open");
	static Persistent<String> read_sym        = NODE_PSYMBOL("read");
	static Persistent<String> write_sym       = NODE_PSYMBOL("write");
	static Persistent<String> flush_sym       = NODE_PSYMBOL("flush");
	static Persistent<String> release_sym     = NODE_PSYMBOL("release");
	static Persistent<String> fsync_sym       = NODE_PSYMBOL("fsync");
	static Persistent<String> opendir_sym     = NODE_PSYMBOL("opendir");
	static Persistent<String> readdir_sym     = NODE_PSYMBOL("readdir");
	static Persistent<String> releasedir_sym  = NODE_PSYMBOL("releasedir");
	static Persistent<String> fsyncdir_sym    = NODE_PSYMBOL("fsyncdir");
	static Persistent<String> statfs_sym      = NODE_PSYMBOL("statfs");
	static Persistent<String> setxattr_sym    = NODE_PSYMBOL("setxattr");
	static Persistent<String> getxattr_sym    = NODE_PSYMBOL("getxattr");
	static Persistent<String> listxattr_sym   = NODE_PSYMBOL("listxattr");
	static Persistent<String> removexattr_sym = NODE_PSYMBOL("removexattr");
	static Persistent<String> access_sym      = NODE_PSYMBOL("access");
	static Persistent<String> create_sym      = NODE_PSYMBOL("create");
	static Persistent<String> getlk_sym       = NODE_PSYMBOL("getlk");
	static Persistent<String> setlk_sym       = NODE_PSYMBOL("setlk");
	static Persistent<String> bmap_sym        = NODE_PSYMBOL("bmap");
	static Persistent<String> ioctl_sym       = NODE_PSYMBOL("ioctl");
	static Persistent<String> poll_sym        = NODE_PSYMBOL("poll");

	// Major version of the fuse protocol
	static Persistent<String> conn_info_proto_major_sym     = NODE_PSYMBOL("proto_major");
	// Minor version of the fuse protocol
	static Persistent<String> conn_info_proto_minor_sym     = NODE_PSYMBOL("proto_minor");
	// Is asynchronous read supported
	static Persistent<String> conn_info_async_read_sym      = NODE_PSYMBOL("async_read");
	// Maximum size of the write buffer
	static Persistent<String> conn_info_max_write_sym       = NODE_PSYMBOL("max_write");
	// Maximum readahead
	static Persistent<String> conn_info_max_readahead_sym   = NODE_PSYMBOL("max_readahead");
	// Capability flags, that the kernel supports
	static Persistent<String> conn_info_capable_sym         = NODE_PSYMBOL("capable");
	// Capability flags, that the filesystem wants to enable
	static Persistent<String> conn_info_want_sym            = NODE_PSYMBOL("want");

	void FileSystem::Initialize() {
		fuse_ops.init       = FileSystem::Init;
		fuse_ops.destroy    = FileSystem::Destroy;
		fuse_ops.lookup     = FileSystem::Lookup;
		fuse_ops.forget     = FileSystem::Forget;
		fuse_ops.getattr    = FileSystem::GetAttr;
		fuse_ops.setattr    = FileSystem::SetAttr;
		fuse_ops.readlink   = FileSystem::ReadLink;
		fuse_ops.mknod      = FileSystem::MkNod;
		fuse_ops.mkdir      = FileSystem::MkDir;
		fuse_ops.unlink     = FileSystem::Unlink;
		fuse_ops.rmdir      = FileSystem::RmDir;
		fuse_ops.symlink    = FileSystem::SymLink;
		fuse_ops.rename     = FileSystem::Rename;
		fuse_ops.link       = FileSystem::Link;
		fuse_ops.open       = FileSystem::Open;
		fuse_ops.read       = FileSystem::Read;
		fuse_ops.write      = FileSystem::Write;
		fuse_ops.flush      = FileSystem::Flush;
		fuse_ops.release    = FileSystem::Release;
		fuse_ops.fsync      = FileSystem::FSync;
		fuse_ops.opendir    = FileSystem::OpenDir;
		fuse_ops.readdir    = FileSystem::ReadDir;
		fuse_ops.releasedir = FileSystem::ReleaseDir;
		fuse_ops.fsyncdir   = FileSystem::FSyncDir;
		fuse_ops.statfs     = FileSystem::StatFs;
		fuse_ops.setxattr   = FileSystem::SetXAttr;
		fuse_ops.getxattr   = FileSystem::GetXAttr;
		fuse_ops.listxattr  = FileSystem::ListXAttr;
		fuse_ops.removexattr = FileSystem::RemoveXAttr;
		fuse_ops.access     = FileSystem::Access;
		fuse_ops.create     = FileSystem::Create;
		fuse_ops.getlk      = FileSystem::GetLock;
		fuse_ops.setlk      = FileSystem::SetLock;
		fuse_ops.bmap       = FileSystem::BMap;
		// fuse_ops.ioctl      = FileSystem::IOCtl;
		// fuse_ops.poll       = FileSystem::Poll;
	}

	void FileSystem::Proxy(void *pUserdata, void *pArgs, const char *pName) {
		Userdata *_userdata = reinterpret_cast<Userdata *>(pUserdata);
		ThreadFunData *data = new ThreadFunData();

		fprintf(stderr, "op name %s\n", pName);

		data->op = pName;
		data->args = (void **)pArgs;
		_userdata->async->data = data;

		uv_async_send(_userdata->async);
	}

	void FileSystem::Init(void *userdata,
	                      struct fuse_conn_info *conn) {

		void **args = new void *[1];
		args[0] = (void *)conn;

		FileSystem::Proxy(userdata, args, "Init");
	}

	void FileSystem::Destroy(void *userdata) {
		void **args = new void *[0];
		FileSystem::Proxy(userdata, args, "Destroy");
	}

	void FileSystem::Lookup(fuse_req_t req,
	                        fuse_ino_t parent,
	                        const char *name) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Lookup");
	}

	void FileSystem::Forget(fuse_req_t req,
	                        fuse_ino_t ino,
	                        unsigned long nlookup) {
		
		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)nlookup;

		fprintf(stderr, "0 %p\n", &args[0]);
		fprintf(stderr, "0 %p\n", &args[1]);
		fprintf(stderr, "0 %p\n", &args[2]);

		// FileSystem::Proxy(fuse_req_userdata(req), args, "Forget");

		fuse_reply_none(req);
	}

	void FileSystem::GetAttr(fuse_req_t req,
	                         fuse_ino_t ino,
	                         struct fuse_file_info *fi) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "GetAttr");
	}

	void FileSystem::SetAttr(fuse_req_t req,
	                         fuse_ino_t ino,
	                         struct stat *attr,
	                         int to_set,
	                         struct fuse_file_info *fi) {

		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)attr;
		args[3] = (void *)to_set;
		args[4] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "SetAttr");
	}

	void FileSystem::ReadLink(fuse_req_t req,
	                          fuse_ino_t ino) {

		void **args = new void *[2];
		args[0] = (void *)req;
		args[1] = (void *)ino;

		FileSystem::Proxy(fuse_req_userdata(req), args, "ReadLink");
	}

	void FileSystem::MkNod(fuse_req_t req,
	                       fuse_ino_t parent,
	                       const char *name,
	                       mode_t mode,
	                       dev_t rdev) {

		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;
		args[3] = (void *)mode;
		args[4] = (void *)rdev;

		FileSystem::Proxy(fuse_req_userdata(req), args, "MkNod");
	}

	void FileSystem::MkDir(fuse_req_t req,
	                       fuse_ino_t parent,
	                       const char *name,
	                       mode_t mode) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;
		args[3] = (void *)mode;

		FileSystem::Proxy(fuse_req_userdata(req), args, "MkDir");
	}

	void FileSystem::Unlink(fuse_req_t req,
	                        fuse_ino_t parent,
	                        const char *name) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Unlink");
	}

	void FileSystem::RmDir(fuse_req_t req,
	                       fuse_ino_t parent,
	                       const char *name) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;

		FileSystem::Proxy(fuse_req_userdata(req), args, "RmDir");
	}

	void FileSystem::SymLink(fuse_req_t req,
	                         const char *link,
	                         fuse_ino_t parent,
	                         const char *name) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)link;
		args[2] = (void *)parent;
		args[3] = (void *)name;

		FileSystem::Proxy(fuse_req_userdata(req), args, "SymLink");
	}

	void FileSystem::Rename(fuse_req_t req,
	                        fuse_ino_t parent,
	                        const char *name,
	                        fuse_ino_t newparent,
	                        const char *newname) {

		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;
		args[3] = (void *)newparent;
		args[4] = (void *)newname;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Rename");
	}

	void FileSystem::Link(fuse_req_t req,
	                      fuse_ino_t ino,
	                      fuse_ino_t newparent,
	                      const char *newname) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)newparent;
		args[3] = (void *)newname;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Link");
	}

	void FileSystem::Open(fuse_req_t req,
	                      fuse_ino_t ino,
	                      struct fuse_file_info *fi) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Open");
	}

	void FileSystem::Read(fuse_req_t req,
	                      fuse_ino_t ino,
	                      size_t size_,
	                      off_t off,
	                      struct fuse_file_info *fi) {


		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)size_;
		args[3] = (void *)off;
		args[4] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Read");
	}

	void FileSystem::Write(fuse_req_t req,
	                       fuse_ino_t ino,
	                       const char *buf,
	                       size_t size,
	                       off_t off,
	                       struct fuse_file_info *fi) {

		void **args = new void *[6];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)buf;
		args[3] = (void *)size;
		args[4] = (void *)off;
		args[5] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Write");
	}

	void FileSystem::Flush(fuse_req_t req,
	                       fuse_ino_t ino,
	                       struct fuse_file_info *fi) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Flush");
	}

	void FileSystem::Release(fuse_req_t req,
	                         fuse_ino_t ino,
	                         struct fuse_file_info *fi) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Release");
	}

	void FileSystem::FSync(fuse_req_t req,
	                       fuse_ino_t ino,
	                       int datasync_,
	                       struct fuse_file_info *fi) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)datasync_;
		args[3] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "FSync");
	}

	void FileSystem::OpenDir(fuse_req_t req,
	                         fuse_ino_t ino,
	                         struct fuse_file_info *fi) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "OpenDir");
	}

	void FileSystem::ReadDir(fuse_req_t req,
	                         fuse_ino_t ino,
	                         size_t size_,
	                         off_t off,
	                         struct fuse_file_info *fi) {

		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)size_;
		args[3] = (void *)off;
		args[4] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "ReadDir");
	}

	void FileSystem::ReleaseDir(fuse_req_t req,
	                            fuse_ino_t ino,
	                            struct fuse_file_info *fi) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "ReleaseDir");
	}

	void FileSystem::FSyncDir(fuse_req_t req,
	                          fuse_ino_t ino,
	                          int datasync_,
	                          struct fuse_file_info *fi) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)datasync_;
		args[3] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "FSyncDir");
	}

	void FileSystem::StatFs(fuse_req_t req,
	                        fuse_ino_t ino) {

		void **args = new void *[2];
		args[0] = (void *)req;
		args[1] = (void *)ino;

		FileSystem::Proxy(fuse_req_userdata(req), args, "StatFs");
	}

	void FileSystem::SetXAttr(fuse_req_t req,
	                          fuse_ino_t ino,
	                          const char *name_,
	                          const char *value_,
	                          size_t size_,
#ifdef __APPLE__
	                          int flags_,
	                          uint32_t position_) {
#else
	                          int flags_) {
#endif

#ifdef __APPLE__
		void **args = new void *[6];
#else
		void **args = new void *[5];
#endif

		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)name_;
		args[3] = (void *)value_;
		args[4] = (void *)size_;

#ifdef __APPLE__
		args[5] = (void *)flags_;
		args[6] = (void *)position_;
#else
		args[5] = (void *)flags_;
#endif

		FileSystem::Proxy(fuse_req_userdata(req), args, "SetXAttr");
	}

	void FileSystem::GetXAttr(fuse_req_t req,
	                          fuse_ino_t ino,
	                          const char *name_,
	                          size_t size_
#ifdef __APPLE__
	                          , uint32_t position_) {
#else
	                         ) {
#endif
		
#ifdef __APPLE__
		void **args = new void *[5];
#else
		void **args = new void *[4];
#endif

		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)name_;
		args[3] = (void *)size_;

#ifdef __APPLE__
		args[4] = (void *)position_;
#else
#endif

		FileSystem::Proxy(fuse_req_userdata(req), args, "GetXAttr");
	}

	void FileSystem::ListXAttr(fuse_req_t req,
	                           fuse_ino_t ino,
	                           size_t size_) {
		
		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)size_;

		FileSystem::Proxy(fuse_req_userdata(req), args, "ListXAttr");
	}

	void FileSystem::RemoveXAttr(fuse_req_t req,
	                             fuse_ino_t ino,
	                             const char *name_) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)name_;

		FileSystem::Proxy(fuse_req_userdata(req), args, "RemoveXAttr");
	}

	void FileSystem::Access(fuse_req_t req,
	                        fuse_ino_t ino,
	                        int mask_) {

		void **args = new void *[3];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)mask_;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Access");
	}

	void FileSystem::Create(fuse_req_t req,
	                        fuse_ino_t parent,
	                        const char *name,
	                        mode_t mode,
	                        struct fuse_file_info *fi) {
		
		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)parent;
		args[2] = (void *)name;
		args[3] = (void *)mode;
		args[4] = (void *)fi;

		FileSystem::Proxy(fuse_req_userdata(req), args, "Create");
	}

	void FileSystem::GetLock(fuse_req_t req,
	                         fuse_ino_t ino,
	                         struct fuse_file_info *fi,
	                         struct flock *lock) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;
		args[3] = (void *)lock;

		FileSystem::Proxy(fuse_req_userdata(req), args, "GetLock");
	}

	void FileSystem::SetLock(fuse_req_t req,
	                         fuse_ino_t ino,
	                         struct fuse_file_info *fi,
	                         struct flock *lock,
	                         int sleep_) {

		void **args = new void *[5];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)fi;
		args[3] = (void *)lock;
		args[4] = (void *)sleep_;

		FileSystem::Proxy(fuse_req_userdata(req), args, "SetLock");
	}

	void FileSystem::BMap(fuse_req_t req,
	                      fuse_ino_t ino,
	                      size_t blocksize_,
	                      uint64_t idx) {

		void **args = new void *[4];
		args[0] = (void *)req;
		args[1] = (void *)ino;
		args[2] = (void *)blocksize_;
		args[3] = (void *)idx;

		FileSystem::Proxy(fuse_req_userdata(req), args, "BMap");
	}

	void FileSystem::IOCtl(fuse_req_t req,
	                       fuse_ino_t ino,
	                       int cmd,
	                       void *arg,
	                       struct fuse_file_info *fi,
	                       unsigned *flagsp,
	                       const void *in_buf,
	                       size_t in_bufsz,
	                       size_t out_bufszp) {

	}

	void FileSystem::Poll(fuse_req_t req,
	                      fuse_ino_t ino,
	                      struct fuse_file_info *fi,
	                      struct fuse_pollhandle *ph) {


	}

	struct fuse_lowlevel_ops *FileSystem::GetOperations() {
		return &fuse_ops;
	}
}