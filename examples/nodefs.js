/**
 * NodeFS filesystem
 **/

var FileSystem = require('../fuse').FileSystem;
var PosixError = require('../fuse').PosixError;
var util = require('util');
var path = require('path');
var fs = require('fs');

var NodeFS = function(fuse, options) {
	this.fuse = fuse;
	this.options = options;

	this.generation = 1;
	this.attr_timeout = 0;
	this.entry_timeout = 0;

	FileSystem.call(this);
};

util.inherits(NodeFS, FileSystem);

(function() {
	var self = this;

	this.test_path = '/tmp';
	this.tree = { 1: this.test_path + '/' };

	this.hash = function(str) {
		var hash = 0,
			c = '';

		for (i = 0; i < str.length; i++) {
			c = str.charCodeAt(i);
			hash = ((hash << 5) - hash) + c;
			hash = hash & hash;
		}
		return Math.abs(hash);
	};

	this.init = function(connInfo) {
		console.log('init', 'Initializing NodeFS filesystem!');
		console.log('init', self.options);
	};

	this.destroy = function() {
		console.log('destroy', 'Cleaning up filesystem...');
	};

	this.lookup = function(context, parent, name, reply) {
		var fspath = path.join(self.tree[parent], name);
		var inode = self.hash(fspath);


		console.log('lookup, inode', inode, context.pid);
		console.log('lookup, path', fspath);


		try {
			var stats = fs.lstatSync(fspath);
		} catch(err) {
			return reply.err(PosixError.ENOENT);
		}

		var entry = {
			inode: inode,
			attr: stats,
			generation: this.generation,
			attr_timeout: this.attr_timeout,
			entry_timeout: this.entry_timeout
		};

		reply.entry(entry);
		self.tree[inode] = fspath;
	};

	this.forget = function(context, inode, nlookup) {
		console.log('forget', context, inode, nlookup);
	};

	this.getattr = function(context, inode, reply) {
		console.log('getattr, inode', inode, context.pid);

		var fspath = self.tree[inode];

		try {
			var stats = fs.lstatSync(fspath);
		} catch(err) {
			return reply.err(PosixError.ENOENT);
		}

		stats.ino = inode;
		stats.inode = inode;

		reply.attr(stats);
	};

	this.setattr = function(context, inode, attrs, reply) {
		console.log('Setattr was called!!');

		reply.err(PosixError.EIO);
	};

	this.mknod = function(context, parent, name, mode, rdev, reply) {
		console.log('Mknod was called!');
		reply.err(PosixError.ENOENT);
		//reply.entry(entry);
	};

	this.mkdir = function(context, parent, name, mode, reply) {
		console.log('mkdir', context.pid);
		console.log('mkdir', arguments);

		var fspath = path.join(self.tree[parent], name);
		var exception = fs.mkdirSync(fspath, mode);
		var inode = self.hash(fspath);

		if (exception) {
			console.log('mkdir, exception', exception);
			return reply.err(PosixError.EIO);
		}
		
		var stats = fs.statSync(fspath);
		
		self.tree[inode] = fspath;
		stats.ino = inode;
		stats.inode = inode;

		var entry = {
			inode: inode,
			attr: stats,
			generation: this.generation,
			attr_timeout: this.attr_timeout,
			entry_timeout: this.entry_timeout
		};

		reply.entry(entry);
	};

	this.unlink = function(context, parent, name, reply) {
		console.log('unlink', context.pid);

		var fspath = path.join(self.tree[parent], name);
		var exception = fs.unlinkSync(fspath);
		var inode = self.hash(fspath);

		if (exception) {
			console.log('unlink, exception', exception);
			return reply.err(PosixError.EIO);
		}

		delete self.tree[inode];

		reply.err(0);
	};

	this.rmdir = function(context, parent, name, reply) {
		console.log('rmdir', context.pid);

		var fspath = path.join(self.tree[parent], name);
		var exception = fs.rmdir(fspath);
		var inode = self.hash(fspath);

		if (exception) {
			console.log('rmdir, exception', exception);
			return reply.err(PosixError.EIO);
		}

		delete self.tree[inode];

		// CHILD DIRS...

		reply.err(0);
	};

	this.link = function(context, inode, newParent, newName, reply) {
		// console.log('Link was called!');
		// reply.err(PosixError.EIO);
		// reply.entry(entry);

		console.log('link, inode', inode);
		console.log('link', arguments);

		var src_path = self.tree[inode];
		var dst_path = path.join(self.tree[newParent], newName);
		var inode = self.hash(dst_path);

		var exception = fs.linkSync(src_path, dst_path);

		if (exception) {
			console.log('link, exception', exception);
			return reply.err(PosixError.EIO);
		}

		var stats = fs.statSync(dst_path);

		self.tree[inode] = dst_path;
		stats.ino = inode;
		stats.inode = inode;

		var entry = {
			inode: inode,
			attr: stats,
			generation: this.generation,
			attr_timeout: this.attr_timeout,
			entry_timeout: this.entry_timeout
		};

		reply.entry(entry);
	};

	this.symlink = function(context, parent, link, name, reply) {
		console.log('symlink, parent', parent);

		var src_path = link;
		var dst_path = path.join(self.tree[parent], name);
		var inode = self.hash(dst_path);

		var exception = fs.symlinkSync(src_path, dst_path);

		if (exception) {
			console.log('symlink, exception', exception);
			return reply.err(PosixError.EIO);
		}

		var stats = fs.lstatSync(dst_path);

		self.tree[inode] = dst_path;
		stats.ino = inode;
		stats.inode = inode;

		var entry = {
			inode: inode,
			attr: stats,
			generation: this.generation,
			attr_timeout: this.attr_timeout,
			entry_timeout: this.entry_timeout
		};

		reply.entry(entry);
	};

	this.readlink = function(context, inode, reply) {
		console.log('readlink, inode', inode);

		var dst_path = self.tree[inode];
		var src_path = fs.readlinkSync(dst_path);

		reply.readlink(src_path);
	};

	this.rename = function(context, parent, name, newParent, newName, reply) {
		var old_path = path.join(self.tree[parent], name);
		var new_path = path.join(self.tree[newParent], newName);
		var new_inode = self.hash(new_path);

		fs.renameSync(old_path, new_path);
		reply.err(0);
		
		self.tree[new_inode] = new_path;

		console.log('rename', old_path, new_path);
	};

	this.open = function(context, inode, fileInfo, reply) {
		console.log('open, inode', inode, context.pid);

		reply.open(fileInfo);
	};

	this.read = function(context, inode, size, offset, fileInfo, reply) {
		var fspath = self.tree[inode];
		var buf = fs.readFileSync(fspath);

		reply.buffer(buf);

		console.log('read, inode', inode, context.pid, fspath);
	};

	this.write = function(context, inode, buffer, offset, fileInfo, reply) {
		var fspath = self.tree[inode];
		var buf = fs.readFileSync(fspath);

		fs.writeFileSync(fspath, Buffer.concat([buf.slice(0, offset), buffer]));

		reply.write(buffer.length);

		console.log('write, inode', inode, context.pid);
	};

	this.flush = function(context, inode, fileInfo, reply) {
		console.log('flush, inode', inode, context.pid);
		reply.err(0);
	};

	this.release = function(context, inode, fileInfo, reply) {
		console.log('release, inode', inode, context.pid);
		reply.err(0);
	};

	//if datasync is true then only user data is flushed, not metadata
	this.fsync = function(context, inode, datasync, fileInfo, reply) {
		console.log('Fsync was called!');
		console.log('datasync -> ' + datasync);
		reply.err(0);
	};

	this.opendir = function(context, inode, fileInfo, reply) {
		console.log('opendir, inode', inode, context.pid);

		reply.open(fileInfo);
	};

	this.readdir = function(context, inode, size, offset, fileInfo, reply) {
		console.log('readdir, inode', inode, context.pid);

		var fspath = self.tree[inode];
		var entries = ['..', '.'].concat(fs.readdirSync(fspath));

		if (entries.length <= offset + 1)
			return reply.buffer(new Buffer(''));

		for (var i = 0, len = entries.length; i < len; i++) {
			var stat = fs.lstatSync(path.join(fspath, entries[i]));
			var entry_path = path.join(fspath, entries[i]);
			var entry_inode = self.hash(entry_path);

			self.tree[entry_inode] = entry_path;

			stat.inode = entry_inode;
			reply.addDirEntry(entries[i], size, stat, offset + i);
		}

		reply.buffer(new Buffer(''));
	};

	this.releasedir = function(context, inode, fileInfo, reply) {
		console.log('releasedir, inode', inode);
		reply.err(0);
	};

	//if datasync is true then only directory contents is flushed, not metadata
	this.fsyncdir = function(context, inode, datasync, fileInfo, reply) {
		console.log('FsyncDir was called!');
		console.log('datasync -> ' + datasync);
		reply.err(0);
	};

	this.statfs = function(context, inode, reply) {
		console.log('statfs, inode', inode);

		var statvfs = {
			bsize: 1024,
			/* file system block size */
			frsize: 0,
			/* fragment size */
			blocks: 0,
			/* size of fs in f_frsize units */
			bfree: 0,
			/* # free blocks */
			bavail: 0,
			/* # free blocks for unprivileged users */
			files: 5,
			/* # inodes */
			ffree: 2,
			/* # free inodes */
			favail: 2,
			/* # free inodes for unprivileged users */
			fsid: 4294967295,
			/* file system ID */
			flag: 0,
			/* mount flags */
			namemax: 1.7976931348623157e+308 /* maximum filename length */
		};

		reply.statfs(statvfs);
	};

	this.setxattr = function(context, inode, name, value, size, flags, position, reply) {
		console.log('setxattr, inode', inode);

		reply.err(0);
	};

	this.getxattr = function(context, inode, name, size, position, reply) {
		console.log('getxattr, inode', inode);

		if (typeof position === 'object') {
			reply = position;
		}

		reply.xattr(size);
	};

	this.listxattr = function(context, inode, size, reply) {
		console.log('listxattr, inode', inode);
		
		reply.err(0);

		// reply.buffer(new Buffer('list,of,extended,attributes'));
		// reply.xattr(1024);
	};

	this.removexattr = function(context, inode, name, reply) {
		console.log('RemoveXAttr was called!');
		console.log(name);

		reply.err(0);
	};

	this.access = function(context, inode, mask, reply) {
		console.log('access, inode', inode, mask);
		reply.err(0);
	};

	this.create = function(context, parent, name, mode, fileInfo, reply) {
		console.log('create, parent', parent);
		console.log('create', arguments);

		var fspath = path.join(self.tree[parent], name);
		var fd = fs.openSync(fspath, 'w+', mode);
		var inode = self.hash(fspath);

		if (isNaN(fd)) {
			console.log('create, exception', fd);
			return reply.err(PosixError.EIO);
		}
		

		var stats = fs.statSync(fspath);

		self.tree[inode] = fspath;
		stats.ino = inode;
		stats.inode = inode;

		var entry = {
			inode: inode,
			attr: stats,
			generation: this.generation,
			attr_timeout: this.attr_timeout,
			entry_timeout: this.entry_timeout
		};

		reply.create(entry, fileInfo);
	};

	this.getlk = function(context, inode, fileInfo, lock, reply) {
		console.log('GetLock was called!');
		console.log('Lock -> ' + lock);
		//reply.lock(lock);
		reply.err(0);
	};

	this.setlk = function(context, inode, fileInfo, lock, sleep, reply) {
		console.log('SetLock was called!!');
		console.log('Lock -> ' + lock);
		console.log('sleep -> ' + sleep);
		reply.err(0);
	};

	this.bmap = function(context, inode, blocksize, index, reply) {
		console.log('BMap was called!');
		//reply.err(0);
		reply.bmap(12344);
	};

	this.ioctl = function() {

	};

	this.poll = function() {

	};
}).call(NodeFS.prototype);

module.exports = NodeFS;