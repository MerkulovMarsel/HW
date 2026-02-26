#include "inode.h"

#define FUSE_USE_VERSION 317

#include <dirent.h>
#include <fuse_lowlevel.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "http.h"
#include "util.h"

static constexpr fuse_ino_t SERVER_ROOT_ID = 1000;
static constexpr size_t DIR_SIZE = 4096;
static constexpr size_t BLK_SIZE = 512;

static std::string token;
static std::map<fuse_ino_t, struct stat> files_attr;

using file_buffer = std::vector<char>;

enum class op { LOOKUP, LIST, CREATE, UNLINK, RMDIR, READ, WRITE, LINK };

template <op op>
struct resp;

template <>
struct resp<op::LOOKUP> {
  unsigned char entry_type;
  ino_t ino;

  static constexpr auto op_name = "lookup";
};

template <>
struct resp<op::LIST> {
  size_t entries_count;
  struct entry {
    unsigned char entry_type;
    ino_t ino;
    char name[256];
  } entries[16];

  static constexpr auto op_name = "list";
};

template <>
struct resp<op::CREATE> {
  ino_t ino;
  static constexpr auto op_name = "create";
};

template <>
struct resp<op::RMDIR> {
  static constexpr auto op_name = "rmdir";
};

template <>
struct resp<op::READ> {
  uint64_t content_length;
  char content[MAX_FILE_SIZE];

  static constexpr auto op_name = "read";
};

template <>
struct resp<op::WRITE> {
  static constexpr auto op_name = "write";
};

template <>
struct resp<op::LINK> {
  ino_t ino;
  static constexpr auto op_name = "link";
};

template <>
struct resp<op::UNLINK> {
  static constexpr auto op_name = "unlink";
};

fuse_ino_t fuse_to_server(fuse_ino_t ino) {
  if (ino == FUSE_ROOT_ID) return SERVER_ROOT_ID;
  return ino;
}

std::string fuse_to_s_server(fuse_ino_t ino) {
  return std::to_string(fuse_to_server(ino));
}

constexpr fuse_ino_t server_to_fuse(fuse_ino_t server_ino) {
  if (server_ino == SERVER_ROOT_ID) return FUSE_ROOT_ID;
  return server_ino;
}

bool check_server_error(networkfs_status result, fuse_req_t req) {
  if (result != NFS_SUCCESS) {
    switch (result) {
      case NFS_ENOENT:
        fuse_reply_err(req, ENOENT);
        break;
      case NFS_ENOTFILE:
        fuse_reply_err(req, EINVAL);
        break;
      case NFS_ENOTDIR:
        fuse_reply_err(req, ENOTDIR);
        break;
      case NFS_ENOENT_DIR:
        fuse_reply_err(req, ENOENT);
        break;
      case NFS_EEXIST:
        fuse_reply_err(req, EEXIST);
        break;
      case NFS_EFBIG:
        fuse_reply_err(req, EFBIG);
        break;
      case NFS_ENOSPC_DIR:
        fuse_reply_err(req, ENOSPC);
        break;
      case NFS_ENOTEMPTY:
        fuse_reply_err(req, ENOTEMPTY);
        break;
      case NFS_ENAMETOOLONG:
        fuse_reply_err(req, ENAMETOOLONG);
        break;
      default:;
    }
    return true;
  }
  return false;
}

enum attr_flags {
  ATTR_DEFAULT = 0,
  ATTR_SET_SIZE = 1 << 0,
  ATTR_SET_MODE = 1 << 1,
  ATTR_SET_NLINK = 1 << 2,
  ATTR_SET_TIMES = 1 << 3
};

void update_size(struct stat* attr, size_t size) {
  attr->st_size = size;
  attr->st_blocks = (attr->st_size + BLK_SIZE - 1) / BLK_SIZE;
}

void fill_attr(fuse_ino_t ino, unsigned char entry_type,
               int flags = ATTR_DEFAULT, off_t size = 0, mode_t mode = 0,
               nlink_t nlink = 0) {
  struct stat attr{};
  attr.st_ino = fuse_to_server(ino);

  if (entry_type == DT_DIR) {
    attr.st_mode =
        (flags & ATTR_SET_MODE) ? (S_IFDIR | (mode & 0777)) : (S_IFDIR | 0755);
    update_size(&attr, (flags & ATTR_SET_SIZE) ? size : DIR_SIZE);
  } else {
    attr.st_mode =
        (flags & ATTR_SET_MODE) ? (S_IFREG | (mode & 0777)) : (S_IFREG | 0644);
    update_size(&attr, (flags & ATTR_SET_SIZE) ? size : 0);
  }
  attr.st_nlink = (flags & ATTR_SET_NLINK) ? nlink : 1;
  attr.st_blksize = BLK_SIZE;
  attr.st_uid = getuid();
  attr.st_gid = getgid();
  attr.st_atime = attr.st_mtime = attr.st_ctime = time(nullptr);
  files_attr[ino] = attr;
}

template <op Op>
std::optional<resp<Op>> call(
    fuse_req_t req,
    const std::vector<std::pair<std::string, std::string>>& args) {
  std::vector<std::byte> response_buffer(sizeof(resp<Op>));

  int64_t result = networkfs_http_call(
      token.c_str(), resp<Op>::op_name,
      reinterpret_cast<char*>(response_buffer.data()), response_buffer.size(),
      std::span<const std::pair<std::string, std::string>>(args.data(),
                                                           args.size()));

  if (check_server_error(static_cast<networkfs_status>(result), req))
    return std::nullopt;

  return std::move(*reinterpret_cast<resp<Op>*>(response_buffer.data()));
}

void networkfs_init(void* userdata, struct fuse_conn_info* conn) {
  (void)conn;

  if (userdata) {
    token = static_cast<char*>(userdata);
  }

  fill_attr(FUSE_ROOT_ID, DT_DIR, ATTR_DEFAULT);
}

void networkfs_destroy(void* private_data) { free(private_data); }

void networkfs_lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
  auto response = call<op::LOOKUP>(
      req, {{"parent", fuse_to_s_server(parent)}, {"name", name}});
  if (!response) return;

  fuse_entry_param e{};
  e.ino = server_to_fuse(response->ino);

  if (!files_attr.contains(e.ino)) {
    fill_attr(e.ino, response->entry_type);
  }

  e.attr = files_attr[e.ino];

  fuse_reply_entry(req, &e);
}

void networkfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info*) {
  if (files_attr.contains(ino)) {
    fuse_reply_attr(req, &files_attr[ino], 0.0);
  } else {
    fuse_reply_err(req, ENOENT);
  }
}

void networkfs_iterate(fuse_req_t req, fuse_ino_t i_ino, size_t size, off_t off,
                       fuse_file_info*) {
  auto response = call<op::LIST>(req, {{"inode", fuse_to_s_server(i_ino)}});
  if (!response) return;

  std::vector<char> buf(size);
  size_t cur_off = 0;

  for (auto i = static_cast<size_t>(off); i < response->entries_count; ++i) {
    const auto& entry = response->entries[i];
    fuse_ino_t entry_ino = server_to_fuse(entry.ino);

    if (!files_attr.contains(entry_ino)) fill_attr(entry_ino, entry.entry_type);

    size_t entry_size =
        fuse_add_direntry(req, buf.data() + cur_off, size - cur_off, entry.name,
                          &files_attr[entry_ino], i + 1);

    if (cur_off + entry_size > size) break;
    cur_off += entry_size;
  }

  fuse_reply_buf(req, buf.data(), cur_off);
}

void networkfs_create(fuse_req_t req, fuse_ino_t parent, const char* name,
                      mode_t mode, fuse_file_info* fi) {
  auto response = call<op::CREATE>(
      req,
      {{"parent", fuse_to_s_server(parent)}, {"name", name}, {"type", "file"}});
  if (!response) return;

  fuse_ino_t entry_ino = server_to_fuse(response->ino);

  fill_attr(entry_ino, DT_REG, ATTR_SET_MODE, 0, mode);

  fuse_entry_param e{};
  e.ino = entry_ino;
  e.attr = files_attr[entry_ino];

  fi->fh = reinterpret_cast<uint64_t>(new file_buffer());
  fuse_reply_create(req, &e, fi);
}

void networkfs_unlink(fuse_req_t req, fuse_ino_t parent, const char* name) {
  auto lookup_response = call<op::LOOKUP>(
      req, {{"parent", fuse_to_s_server(parent)}, {"name", name}});
  if (!lookup_response) return;

  fuse_ino_t file_ino = server_to_fuse(lookup_response->ino);

  auto response = call<op::UNLINK>(
      req, {{"parent", fuse_to_s_server(parent)}, {"name", name}});
  if (!response) return;

  if (files_attr.contains(file_ino)) {
    files_attr[file_ino].st_nlink--;
    files_attr[file_ino].st_ctime = time(nullptr);
    if (files_attr[file_ino].st_nlink == 0) {
      files_attr.erase(file_ino);
    }
  }

  fuse_reply_err(req, 0);
}

void networkfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char* name,
                     mode_t mode) {
  auto response = call<op::CREATE>(req, {{"parent", fuse_to_s_server(parent)},
                                         {"name", name},
                                         {"type", "directory"}});
  if (!response) return;

  fuse_ino_t entry_ino = server_to_fuse(response->ino);

  fill_attr(entry_ino, DT_DIR, ATTR_SET_MODE, 0, mode);

  fuse_entry_param e{};
  e.ino = entry_ino;
  e.attr = files_attr[entry_ino];

  fuse_reply_entry(req, &e);
}

void networkfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name) {
  auto response = call<op::RMDIR>(
      req, {{"parent", fuse_to_s_server(parent)}, {"name", name}});
  if (!response) return;

  fuse_reply_err(req, 0);
}

void networkfs_open(fuse_req_t req, fuse_ino_t i_ino, fuse_file_info* fi) {
  if (!files_attr.contains(i_ino)) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  if (S_ISDIR(files_attr[i_ino].st_mode)) {
    fi->fh = 0;
    fuse_reply_open(req, fi);
    return;
  }

  file_buffer* data = new file_buffer();
  int flags = fi->flags;

  if (!(flags & O_TRUNC)) {
    auto response = call<op::READ>(req, {{"inode", fuse_to_s_server(i_ino)}});

    if (!response) {
      delete data;
      return;
    }

    data->assign(response->content,
                 response->content + response->content_length);

    update_size(&files_attr[i_ino], data->size());
    files_attr[i_ino].st_mtime = time(nullptr);
  }

  fi->fh = reinterpret_cast<uint64_t>(data);
  fuse_reply_open(req, fi);
}

void networkfs_release(fuse_req_t req, fuse_ino_t ino, fuse_file_info* fi) {
  (void)ino;
  if (fi->fh) {
    delete reinterpret_cast<file_buffer*>(fi->fh);
  }
  fuse_reply_err(req, 0);
}

void networkfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                    fuse_file_info* fi) {
  if (fi->fh == 0) {
    fuse_reply_err(req, EBADF);
    return;
  }

  auto* data = reinterpret_cast<file_buffer*>(fi->fh);

  if (data->empty()) {
    auto response = call<op::READ>(req, {{"inode", fuse_to_s_server(ino)},
                                         {"offset", std::to_string(off)},
                                         {"size", std::to_string(size)}});

    if (!response) return;

    size_t to_read = std::min<size_t>(response->content_length, size);
    fuse_reply_buf(req, response->content, to_read);
    return;
  }

  if (static_cast<size_t>(off) >= data->size()) {
    fuse_reply_buf(req, nullptr, 0);
    return;
  }

  size_t to_read = std::min(size, data->size() - static_cast<size_t>(off));
  fuse_reply_buf(req, data->data() + off, to_read);
}

void networkfs_write(fuse_req_t req, fuse_ino_t ino, const char* buffer,
                     size_t size, off_t off, fuse_file_info* fi) {
  if (!fi->fh) {
    fuse_reply_err(req, EBADF);
    return;
  }

  auto* data = reinterpret_cast<file_buffer*>(fi->fh);

  if (fi->flags & O_APPEND) {
    off = static_cast<off_t>(data->size());
  }

  if (fi->flags & O_TRUNC) {
    data->clear();
  }

  if (static_cast<size_t>(off) + size > data->size()) {
    data->resize(static_cast<size_t>(off) + size);
  }

  memcpy(data->data() + off, buffer, size);
  update_size(&files_attr[ino], data->size());
  files_attr[ino].st_mtime = time(nullptr);

  fuse_reply_write(req, size);
}

void networkfs_flush(fuse_req_t req, fuse_ino_t ino, fuse_file_info* fi) {
  if (fi->fh == 0) {
    fuse_reply_err(req, 0);
    return;
  }

  auto* data = reinterpret_cast<file_buffer*>(fi->fh);

  auto response = call<op::WRITE>(
      req, {{"inode", fuse_to_s_server(ino)},
            {"content", std::string(data->data(), data->size())}});
  if (!response) return;

  fuse_reply_err(req, 0);
}

void networkfs_fsync(fuse_req_t req, fuse_ino_t ino, int, fuse_file_info* fi) {
  networkfs_flush(req, ino, fi);
}

void networkfs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat* attr,
                       int to_set, fuse_file_info* fi) {
  if (!files_attr.contains(ino)) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  struct stat* st = &files_attr[ino];

  if (to_set & FUSE_SET_ATTR_SIZE) {
    if (S_ISREG(st->st_mode)) {
      if (fi && fi->fh) {
        auto* data = reinterpret_cast<file_buffer*>(fi->fh);
        data->resize(static_cast<size_t>(attr->st_size));
      }

      update_size(st, attr->st_size);
      st->st_mtime = time(nullptr);
    }
  }

  if (to_set & FUSE_SET_ATTR_MODE) st->st_mode = attr->st_mode;
  if (to_set & FUSE_SET_ATTR_UID) st->st_uid = attr->st_uid;
  if (to_set & FUSE_SET_ATTR_GID) st->st_gid = attr->st_gid;
  if (to_set & FUSE_SET_ATTR_ATIME) st->st_atime = attr->st_atime;
  if (to_set & FUSE_SET_ATTR_MTIME) st->st_mtime = attr->st_mtime;

  fuse_reply_attr(req, st, 0.0);
}

void networkfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
                    const char* name) {
  if (files_attr.contains(ino) && !S_ISREG(files_attr[ino].st_mode)) {
    fuse_reply_err(req, EPERM);
    return;
  }

  auto response = call<op::LINK>(req, {{"source", fuse_to_s_server(ino)},
                                       {"parent", fuse_to_s_server(newparent)},
                                       {"name", name}});
  if (!response) return;

  if (files_attr.contains(ino)) {
    files_attr[ino].st_nlink++;
    files_attr[ino].st_ctime = time(nullptr);
  } else {
    fill_attr(ino, DT_REG, ATTR_SET_NLINK, 0, 0, 2);
  }

  fuse_entry_param e{};
  e.ino = ino;
  e.attr = files_attr[ino];

  fuse_reply_entry(req, &e);
}

void networkfs_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup) {
  (void)ino;
  (void)nlookup;
  fuse_reply_none(req);
}

const struct fuse_lowlevel_ops networkfs_oper = {
    .init = networkfs_init,
    .destroy = networkfs_destroy,
    .lookup = networkfs_lookup,
    .forget = networkfs_forget,
    .getattr = networkfs_getattr,
    .setattr = networkfs_setattr,
    .mkdir = networkfs_mkdir,
    .unlink = networkfs_unlink,
    .rmdir = networkfs_rmdir,
    .link = networkfs_link,
    .open = networkfs_open,
    .read = networkfs_read,
    .write = networkfs_write,
    .flush = networkfs_flush,
    .release = networkfs_release,
    .fsync = networkfs_fsync,
    .readdir = networkfs_iterate,
    .create = networkfs_create,
};