#define FUSE_USE_VERSION 30

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// compile command
// gcc main.c -o mount-archive `pkg-config fuse --cflags --libs` -larchive -ldl
// run command -> mount is the name of the  mounting folder
// ./mount-archive -o tar_path="test2.tar" -f mount

struct myfs_config {
  const char *tar_path;
} conf;

static int do_getattr(const char *path, struct stat *st) {
  printf("[getattr] Called\n");
  printf("\tAttributes of %s requested\n", path);

  // GNU's definitions of the attributes
  // (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
  // 		st_uid: 	The user ID of the file’s owner.
  //		st_gid: 	The group ID of the file.
  //		st_atime: 	This is the last access time for the file.
  //		st_mtime: 	This is the time of the last modification to the
  //contents of the file. 		st_mode: 	Specifies the mode of the file. This
  //includes file type information (see Testing File Type) and the file
  //permission bits (see Permission Bits). 		st_nlink: 	The number of hard links
  //to the file. This count keeps track of how many directories have entries for
  //this file. If the count is ever decremented to zero, then the file itself is
  //discarded as soon 						as no process still holds it open. Symbolic links are not
  //counted in the total. 		st_size:	This specifies the size of a regular
  //file in bytes. For files that are really devices this field isn’t usually
  //meaningful. For symbolic links this specifies the length of the file name
  //the link refers to.

  st->st_uid = getuid();  // The owner of the file/directory is the user who
                          // mounted the filesystem
  st->st_gid = getgid();  // The group of the file/directory is the same as the
                          // group of the user who mounted the filesystem
  st->st_atime =
      time(NULL);  // The last "a"ccess of the file/directory is right now
  st->st_mtime =
      time(NULL);  // The last "m"odification of the file/directory is right now

  if (strcmp(path, "/") == 0) {
    st->st_mode = S_IFDIR | 0755;
    st->st_nlink = 2;  // Why "two" hardlinks instead of "one"? The answer is
                       // here: http://unix.stackexchange.com/a/101536
  } else {
    st->st_mode = S_IFREG | 0644;
    st->st_nlink = 1;
    st->st_size = 1024;
  }

  return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi) {
  printf("--> Getting The List of Files of %s\n", path);

  filler(buffer, ".", NULL, 0);   // Current Directory
  filler(buffer, "..", NULL, 0);  // Parent Directory

  if (strcmp(path, "/") ==
      0)  // If the user is trying to show the files/directories of the root
          // directory show the following
  {
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    r = archive_read_open_filename(a, conf.tar_path, 10240);
    if (r != ARCHIVE_OK) exit(1);
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      printf("%s\\n", archive_entry_pathname(entry));
      filler(buffer, archive_entry_pathname(entry), NULL, 0);
      archive_read_data_skip(a);
    }
    r = archive_read_free(a);
    if (r != ARCHIVE_OK) exit(1);
  }

  return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  printf("--> Trying to read %s, %u, %u\n", path, offset, size);

  char file1Text[] = "Hello World!";
  char *selectedText = NULL;

  // ... //

  if (strcmp(path, "/fisier1.txt") == 0)
    selectedText = file1Text;
  else
    return -1;

  // ... //

  memcpy(buffer, selectedText + offset, size);

  return strlen(selectedText) - offset;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .read = do_read,
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(struct myfs_config, p), v }

static struct fuse_opt myfs_opts[] = {MYFS_OPT("tar_path=%s", tar_path, 0),
                                      FUSE_OPT_END};

int main(int argc, char *argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  memset(&conf, 0, sizeof(conf));

  fuse_opt_parse(&args, &conf, myfs_opts, NULL);

  return fuse_main(args.argc, args.argv, &operations, NULL);
}
