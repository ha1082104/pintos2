#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* 02 ============================== */
#include <stdbool.h>
#include "lib/user/syscall.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
/* ================================= */

static void syscall_handler (struct intr_frame *);

/* 02 ============================== */
static void syscall_halt();
static pid_t syscall_exec(const char*);
static int syscall_wait(pid_t);
static bool syscall_create(const char*, unsigned);
static bool syscall_remove(const char*);
static int syscall_open(const char*);
static int syscall_filesize(int);
static int syscall_read(int, void*, unsigned);
static int syscall_write(int, const void*, unsigned);
static void syscall_seek(int, unsigned);
static unsigned syscall_tell(int);
static void syscall_close(int);
static int check_ptr(void*);
static int check_file_ptr(const char*);
/* ================================= */

void
syscall_init (void)
{
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int check_ptr(void* ptr){
	void *valid;
	if(ptr == NULL) return 0;
	if(!is_user_vaddr(ptr)) return 0;
	valid = pagedir_get_page(thread_current()->pagedir, (const void*)ptr);
	if(valid==NULL) return 0;
	return 1;
}

static int check_file_ptr(const char* file){
	void *valid;
	if(file == NULL) return 0;
	if(file == "") return 0;
	if(!is_user_vaddr(file)) return 0;
	valid = pagedir_get_page(thread_current()->pagedir, (const void*)file);
	if(valid == NULL) syscall_exit(-1);
	return 1;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* 02 ================================== */
  int syscall_num = -1;
  int arg_num, i;
  int arg_arr[3] = {0,};
  void* temp_esp;
  pid_t ret_pid_t;
  int ret_int;
  bool ret_bool;
  unsigned ret_unsigned;

  // get syscall number
  temp_esp = f->esp;
  if(!check_ptr(temp_esp)) syscall_exit(-1);
  syscall_num = *(int*)temp_esp;

  switch(syscall_num){
	  case SYS_HALT:
		  arg_num = 0;
		  break;
	  case SYS_EXIT: case SYS_EXEC: case SYS_WAIT: case SYS_REMOVE:
	  case SYS_OPEN: case SYS_FILESIZE: case SYS_TELL: case SYS_CLOSE:
		  arg_num = 1;
		  break;
	  case SYS_CREATE: case SYS_SEEK:
		  arg_num = 2;
		  break;
	  case SYS_READ: case SYS_WRITE:
		  arg_num = 3;
		  break;
      // invalid syscall number
	  default:
		  syscall_exit(-1);
		  break;
  }

  // get syscall arguments
  for(i=0; i<arg_num; i++){
	  temp_esp += 4;
	  if(!check_ptr(temp_esp)) syscall_exit(-1);
	  arg_arr[i] = *(int*)temp_esp;
  }
  switch(syscall_num){
	  case SYS_HALT:
		  syscall_halt();
		  break;
	  case SYS_EXIT:
		  syscall_exit(arg_arr[0]);
		  break;
	  case SYS_EXEC:
		  ret_int = (int)syscall_exec((const char*)arg_arr[0]);
		  memcpy(&(f->eax), &ret_int, sizeof(ret_int));
		  break;
	  case SYS_WAIT:
		  ret_int = syscall_wait((pid_t)arg_arr[0]);
		  memcpy(&(f->eax), &ret_int, sizeof(ret_int));
		  break;
	  case SYS_CREATE:
		  ret_bool = syscall_create((const char*)arg_arr[0],(unsigned)arg_arr[1]);
		  memset(&(f->eax), 0, 4);
		  memcpy(&(f->eax), &ret_bool, sizeof(ret_bool));
		  break;
	  case SYS_REMOVE:
		  ret_bool = syscall_remove((const char *) arg_arr[0]);
		  memset(&(f->eax), 0, 4);
		  memcpy(&(f->eax), &ret_bool, sizeof(ret_bool));
		  break;
	  case SYS_OPEN:
		  ret_int = syscall_open((const char *)arg_arr[0]);
		  memcpy(&(f->eax), &ret_int, sizeof(ret_int));
		  break;
	  case SYS_FILESIZE:
		  ret_int = syscall_filesize(arg_arr[0]);
		  memcpy(&(f->eax), &ret_int, sizeof(ret_int));
		  break;
	  case SYS_READ:
		  ret_int = syscall_read(arg_arr[0], (void *) arg_arr[1], (unsigned)arg_arr[2]);
		  memcpy(&(f->eax), &ret_int, sizeof(ret_int));
		  break;
	  case SYS_WRITE:
		  ret_int = syscall_write(arg_arr[0], (const void *)arg_arr[1], (unsigned)arg_arr[2]);
		  memcpy(&(f->eax), &ret_int, sizeof(ret_int));
		  break;
	  case SYS_SEEK:
		  syscall_seek(arg_arr[0], (unsigned)arg_arr[1]);
		  break;
	  case SYS_TELL:
		  ret_unsigned = syscall_tell(arg_arr[0]);
		  memcpy(&(f->eax), &ret_unsigned, sizeof(ret_unsigned));
		  break;
	  case SYS_CLOSE:
		  syscall_close(arg_arr[0]);
		  break;
	  default:
		  syscall_exit(-1);
		  break;
  }
  /* ===================================== */
  
  /* original ============================ */
  //printf ("system call!\n");
  //thread_exit ();
  /* ===================================== */
}

/* 02 ==================================== */
static void syscall_halt(void){
	shutdown_power_off();
}

void syscall_exit(int status){
	thread_current()->exit_status = status;
	printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}

static pid_t syscall_exec(const char* file){
	if(!check_file_ptr(file)) syscall_exit(-1);
	return process_execute(file);
}

static int syscall_wait(pid_t pid){
	return process_wait((tid_t) pid);
}

static bool syscall_create(const char *file, unsigned initial_size){
	if(!check_file_ptr(file)) syscall_exit(-1);
	return filesys_create(file, (off_t) initial_size);
}

static bool syscall_remove(const char *file){
	if(!check_file_ptr(file)) syscall_exit(-1);
	return filesys_remove(file);
}

static int syscall_open(const char *file){
	struct file* newfile;
	if(!check_file_ptr(file)) return -1;
	newfile = filesys_open(file);
	if(newfile == NULL) return -1;
	return put_fd_table(newfile);
}

static int syscall_filesize(int fd){
	struct file* target;
	target = get_fd_table(fd);
	if(target == NULL) return -1;
	return file_get_size(target);
}

static int syscall_read(int fd, void *buffer, unsigned size){
	struct file* target;
	int bytes_read = 0;

	if(!check_ptr(buffer)) syscall_exit(-1);

	// read from the keyboard
	if(fd == 0) return sizeof(input_getc());

	// get file
	target = get_fd_table(fd);
	if(target == NULL) syscall_exit(-1);
	if(file_is_writing(target)) return -1;

	// reading
	file_deny_write(target);
	bytes_read = (int)file_read(target, buffer, (off_t) size);
	file_allow_write(target);

	return bytes_read;
}

static int syscall_write(int fd, const void *buffer, unsigned size){
	struct file* target;
	int bytes_write = 0;
	int f_size;

	if(!check_ptr(buffer)) syscall_exit(-1);

	// write to console
	if(fd == 1){
		putbuf(buffer, (size_t)size);
		return (int)size;
	}

	// get file
	target = get_fd_table(fd);
	if(target == NULL) syscall_exit(-1);

	// someone is reading or writing now
	if(file_is_reading(target) || file_is_writing(target)) return 0;

	f_size = syscall_filesize(fd);

	// writing
	file_set_writing(target, true);
	bytes_write = (int)file_write(target, buffer, (off_t)size);
	file_set_writing(target, false);
	return bytes_write;
}

static void syscall_seek(int fd, unsigned position){
	struct file* target;
	if(fd == 0 || fd == 1) return;
	target = get_fd_table(fd);
	if(target == NULL) return;
	file_seek(target, (off_t) position);
}

static unsigned syscall_tell(int fd){
	struct file* target;
	if(fd == 0 || fd == 1) return 0;
	target = get_fd_table(fd);
	if(target == NULL) return 0;
	return (unsigned)file_tell(target);
}

static void syscall_close(int fd){
	struct file* target;
	if(fd == 0 || fd == 1) syscall_exit(-1);
	target = get_fd_table(fd);
	if(target == NULL) syscall_exit(-1);
	file_close(target);
	del_fd_table(fd);
}

/* ======================================== */
