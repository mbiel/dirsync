#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include "dirsynctypes.h"


//TODO - avoid infinite loop

long pathsize = 1024; // this will be the default maximum path size if sysconf fails to give a result
int printoutput = 0; // don't print output by default

static int dirsync(char *, char *);

static void setPathMax() {
  long pathmax;
  pathmax = sysconf(_PC_PATH_MAX);
  if(pathmax != -1) {
    printOutput("Maximum path size changed to %ld\n", pathmax);
    pathsize = pathmax;
  }
  else {
    printOutput("Could not determine maximum path size, retaining default\n\n");
  }
}


/******************************************************************
 * makeAbsPath concatenates the strings given by the dir and file 
 * arguments, with a '/' separator, and returns the new string.
 */
static char *makeAbsPath(char *str, char *dir, char *file) {
  sprintf(str, "%s%c%s", dir, '/', file);
  return str;
}

/******************************************************************
 * printError just prints an error message to stderr. It is called 
 * by giving the name of the function or action that is being 
 * performed, and the argument that is being operated on when 
 * the error arises.
 */
static void printError(char *function, char *arg)
{
  fprintf(stderr,"Error trying to call %s with argument %s: %s\n",function,arg,strerror(errno));
}

/******************************************************************
 * makeDirectory makes a Directory from the filesystem directory 
 * with the name dirname. The dirlist argument is a pointer to 
 * a directory to which fileLists will be added. On error, 
 * makeDirectory returns -1, and on success, it returns 0.
 */
static int makeDirectory(char *dirname, Directory *dirlist) {
  DIR *dir_ptr;
  struct dirent *dirent_ptr;
  struct stat thisstat;
  
  char path[pathsize];
  
  dirlist->files = makeList();
  dirlist->subdirs = makeList();
  
  if((dir_ptr = opendir(dirname)) == NULL) {
    printError("opendir", dirname);
    return -1;
  }
  
  if(lstat(dirname,&thisstat)) {
    printError("stat",dirname);
    return -1;
  }
  
  else {
    
    while((dirent_ptr = readdir(dir_ptr)) != NULL) {
      makeAbsPath(path,dirname,dirent_ptr->d_name);
      
      
      if(lstat(path, &thisstat)) {
	printError("stat",path);
	return -1;  
      }
      
      
      switch(thisstat.st_mode & S_IFMT) {
	//directories are added to the subdirs list
	case S_IFDIR:
	  addFile(dirlist->subdirs, dirent_ptr->d_name, &thisstat);
	  break;
	  
	  //symlinks and files are added to the files list
	case S_IFLNK:
	case S_IFREG:
	  addFile(dirlist->files, dirent_ptr->d_name, &thisstat);
	  break;
	  
	  
	default:
	  printOutput("Ignored unhandled file type %s in directory %s\n",dirent_ptr->d_name, dirname);
	  break;
      }
    }
    closedir(dir_ptr);
  }
  
  return 0;
}
/******************************************************************
 * copyStat copies the permission and time attributes from the stat 
 * struct pointed to by stat to the file pointed to by path.
 */
static void copyStat(char *path, struct stat *stat) {
  
  //To change file protection, use chmod function
  if(chmod(path,stat->st_mode)) {
    printError("chmod",path);
  }
  
  //To change file access and modification times, use the utime function
  struct utimbuf time;
  
  time.modtime = stat->st_mtime;
  time.actime = stat->st_atime;
  
  if(utime(path,&time) < 0) {
    printError("utime",path);
  }
  
}

/******************************************************************
 * copyFile copies the file in the location src to the location dest.
 * It then uses copyStat to change the modification time and permissions
 * to be the same as those of the source file.
 */
static int copyFile(char *src, char *dest, fileItem *file) {
  
  char srcpath[pathsize];
  char destpath[pathsize];
  char linkpath[pathsize];
  
  makeAbsPath(srcpath,src,file->name);
  makeAbsPath(destpath,dest,file->name);
  
  size_t filesize;
  char *buffer;
  
  FILE *fsrc, *fdest;
  
  if((file->itemStat.st_mode & S_IFMT) == S_IFLNK) {
    //it is a symlink
    size_t link;
    printOutput("Copying symlink %s\n",file->name);
    
    link = readlink(srcpath, linkpath, pathsize);
    if((int)link < 0) {
      printError("readlink",src);
      return -1;
    }
    
    linkpath[link] = '\0';
    
    printOutput("Trying to create %s from %s\n\n",destpath,linkpath);
    
    int linkcheck;
    if((linkcheck = symlink(linkpath,destpath)) < 0) {
      printError("symlink",destpath);
      return -1;
    }
    
    return 0;
    
  } else {
    printOutput("Copying file %s of size %ld bytes from %s to %s\n\n", file->name, file->itemStat.st_size, srcpath, destpath);
    
    //open srcpath for reading
    fsrc = fopen(srcpath,"r");
    if(!fsrc) {
      printError("open",srcpath);
      return -1;
    }
    
    //open destpath for writing
    fdest = fopen(destpath, "w");
    if(!fdest) {
      printError("open",destpath);
      fclose(fsrc);
      return -1;
    }
    
    //seek to the end of the file, determine the filesize and rewind to the beginning of the file
    if(fseek(fsrc,0,SEEK_END)) {
      printf("Seek error\n");
      fclose(fsrc);
      fclose(fdest);
      return -1;
    }
    filesize = ftell(fsrc);
    rewind(fsrc);
    
    
    if(!(buffer = malloc(filesize))) {
      char errbuff[128];
      sprintf(errbuff, "%d", filesize); 
      printError("malloc",errbuff);
      fclose(fsrc);
      fclose(fdest);
      return 0;
    }
    
    //read everything into the buffer
    if(fread(buffer,1,filesize,fsrc) < filesize) {
      free(buffer);
      fclose(fsrc);
      fclose(fdest);
      printError("fread",srcpath);
      return -1;
    }
    
    //write it out
    if(fwrite(buffer,1,filesize,fdest) < filesize) {
      free(buffer);
      fclose(fsrc);
      fclose(fdest);
      printError("fwrite",destpath);
      return -1;
    }
    
    free(buffer);
    fclose(fsrc);
    fclose(fdest);
    
    copyStat(destpath, &file->itemStat);
    
    
    return 0;
  }
}

/******************************************************************
 * moveNeededFiles takes four arguments. One Directory is regarded
 * as the source, and the other as the destination. Any files present 
 * in the source that are not in the destination will be copied to the 
 * destination. srcDir and destDir should point to Directory structs, where
 * src and dest are the pathnames to the directories.
 */
static void moveNeededFiles(Directory *srcDir, char *src, Directory *destDir, char *dest) {
  
  fileList *srcFileArray = srcDir->files;
  fileList *destFileArray = destDir->files;
  
  fileItem *srcItem, *destItem;
  int i;
  
  for(i = 0; i < srcDir->files->len; i++) {
    srcItem = srcFileArray->dataStart[i];
    destItem = itemFind(destFileArray, srcItem);
    
    /*If the file was not found, it should be copied and the name should be added to the list of destination files*/
    if(!destItem) {
      printOutput("%s does not exist in destination directory: copying\n", srcItem->name);
      copyFile(src,dest,srcItem);
      addFile(destFileArray, srcItem->name, &srcItem->itemStat);
    }
    
    /*For symlinks, since we do not set the time when we create them, we use what they point to 
     *     in order to determine whether they should be copied*/
    
    else if((srcItem->itemStat.st_mode & S_IFMT) == S_IFLNK) {
      
      size_t link;
      int linkcomp;
      char srcpath[pathsize];
      char destpath[pathsize];
      char srclinkpath[pathsize];
      char destlinkpath[pathsize];
      
      makeAbsPath(srcpath,src,srcItem->name);
      makeAbsPath(destpath,dest,destItem->name);
      
      link = readlink(srcpath, srclinkpath, pathsize);
      
      if((int)link < 0) {
	printError("readlink",src);
	continue;
      }
      
      srclinkpath[link] = '\0'; //null terminate
      
      link = readlink(destpath, destlinkpath, pathsize);
      
      if((int)link < 0) {
	printError("readlink",src);
	continue;
      }
      
      destlinkpath[link] = '\0'; //null terminate
      
      linkcomp = strcmp(srclinkpath,destlinkpath);
      
      /*If they point to the same thing, do nothing */
      if(linkcomp == 0) {
	printOutput("Symlinks %s and %s both point to %s. Doing nothing.\n", srcpath,destpath,srclinkpath);
	continue;
      }
      
      /*Otherwise, copy the newer link to the other directory*/
      else {
	printOutput("Src points to %s\nDest points to %s\nCopying newer symlink.\n", srclinkpath, destlinkpath);
	if(srcItem->itemStat.st_mtime > destItem->itemStat.st_mtime) {
	  continue; // if they do happen to have the same mod time, do nothing.
	}
	
	else if(srcItem->itemStat.st_mtime > destItem->itemStat.st_mtime) {
	  printOutput("Source: %s\n", ctime(&(srcItem->itemStat.st_mtime)));
	  printOutput("Dest: %s\n", ctime(&(destItem->itemStat.st_mtime)));
	  printOutput("Source version of %s newer than destination version: copying\n", destItem->name);
	  copyFile(src,dest,srcItem);
	  
	} else {
	  printOutput("Source: %s\n", ctime(&(srcItem->itemStat.st_mtime)));
	  printOutput("Dest: %s\n", ctime(&(destItem->itemStat.st_mtime)));
	  printOutput("Destination version of %s newer than source version: copying\n", destItem->name);
	  copyFile(dest,src,destItem);
	  
	}
      }
    }
    
    /* For regular files, if the mod times are the same but the sizes are different, 
     *    just print this and do nothing. If the times and sizes are the same, still do nothing. */
    
    else if(srcItem->itemStat.st_mtime == destItem->itemStat.st_mtime) {
      if(srcItem->itemStat.st_size != destItem->itemStat.st_size) {
	printOutput("Error: mod time for file %s and file %s are the same but file sizes are different. Doing nothing\n", srcItem->name, destItem->name);
	continue;
      } else {
	printOutput("File %s is the same in both directories. Doing nothing\n", destItem->name);
	continue;
      }
    }
    
    /* If the modification times are different, copy the more recently modified file into the other directory */
    
    else if(srcItem->itemStat.st_mtime > destItem->itemStat.st_mtime) {
      printOutput("Source: %s\n", ctime(&(srcItem->itemStat.st_mtime)));
      printOutput("Dest: %s\n", ctime(&(destItem->itemStat.st_mtime)));
      printOutput("Source version of %s newer than destination version: copying\n", destItem->name);
      copyFile(src,dest,srcItem);
      addFile(destFileArray, srcItem->name, &srcItem->itemStat);
    } 
    else {
      printOutput("Source: %s\n", ctime(&(srcItem->itemStat.st_mtime)));
      printOutput("Dest: %s\n", ctime(&(destItem->itemStat.st_mtime)));
      printOutput("Destination version of %s newer than source version: copying\n", destItem->name);
      copyFile(dest,src,destItem);
      addFile(srcFileArray, destItem->name, &destItem->itemStat);
    }
  }
  
}
/******************************************************************
 * We do not want to copy a directory into itself or into a subdirectory of
 * itself -- this will cause infinite loops. unsafeToCopy returns 1 if 
 * it will be unsafe to copy the fileItem pointed to by srcItem into the 
 * directory named by destpath. On error, it returns -1. Otherwise, it returns 0. */

static int unsafeToCopy(fileItem *srcItem, char *destpath) {
  DIR *dir_ptr;
  struct dirent *dirent_ptr;
  struct stat thisstat;
  
  char path[pathsize];
  
  if((dir_ptr = opendir(destpath)) == NULL) {
    printError("opendir", destpath);
    return -1;
  }
  
  if(stat(destpath,&thisstat)) {
    printError("stat",destpath);
    return -1;
  }
  
  /*If they have the same ino, they are the same */
  if(thisstat.st_ino == srcItem->itemStat.st_ino) {
    printOutput("Src and dest are the same -- no copy possible\n");
    closedir(dir_ptr);
    return 1;
  }
  
  /*If not, we will keep going up one directory by opening '..' until
   * we either determine that the inode matches something further up 
   * (in which case it is not safe to copy) or [xxx] == [xxx]/..
   * (in which case we are at the root, and if there 
   * haven't been any problems, it's safe to copy) */
  
  else {
    
    while((dirent_ptr = readdir(dir_ptr)) != NULL) {
      if(strcmp("..",dirent_ptr->d_name) == 0)
	break;
    }
    
    if(thisstat.st_ino == dirent_ptr->d_ino) {
      closedir(dir_ptr);
      return 0;
    }
    
    else if(dirent_ptr->d_ino == srcItem->itemStat.st_ino) {
      printOutput("Unsafe to copy a directory to its own subdirectory\n");
      closedir(dir_ptr);
      return 1;
    }
    
    makeAbsPath(path,destpath,dirent_ptr->d_name);
    closedir(dir_ptr);
    return(unsafeToCopy(srcItem,path));
  }
  
  return 0;
}

/******************************************************************
 * moveNeededDirs is like moveNeededFiles, except that it deals with 
 * subdirectories. Any subdirectories present in srcDir but not in destDir
 * will be copied to destDir. src and dest should be the pathnames for 
 * srcDir and destDir, respectively.
 */
static void moveNeededDirs(Directory *srcDir, char *src, Directory *destDir, char *dest) {
  fileList *srcDirArray = srcDir->subdirs;
  fileList *destDirArray = destDir->subdirs;
  
  fileItem *srcItem, *destItem;
  int i;
  
  char srcpath[pathsize],destpath[pathsize];
  
  for(i = 0; i < srcDir->subdirs->len; i++) {
    srcItem = srcDirArray->dataStart[i];
    destItem = itemFind(destDirArray,srcItem);
    
    int unsafe = 0;
    
    //Ignore '.' and '..' directories
    if(strcmp(srcItem->name,".") == 0 || strcmp(srcItem->name,"..") == 0) {
      continue;
    }
    
    makeAbsPath(srcpath,src,srcItem->name);
    makeAbsPath(destpath,dest,srcItem->name);        
    
    
    /*If it was not found, it should be copied and added to the list of destination subdirs
     *	unless it is unsafe -- i.e., copying it would lead to an infinite loop. */
    if(!destItem) {
      
      if(unsafeToCopy(srcItem,dest)) {
	unsafe = 1;
	printOutput("Cannot copy %s to %s\n", srcpath, destpath);
      } else {
	printOutput("%s does not exist in dest directory: copying...\n", srcItem->name);
	mode_t srcmode = srcItem->itemStat.st_mode;
	if(mkdir(destpath,srcmode)) {
	  printError("mkdir", destpath);
	}
	copyStat(destpath, &srcItem->itemStat);
	addFile(destDirArray, srcItem->name, &srcItem->itemStat);
      }
    }
    
    /*If it was found, we don't need to copy, but still need to check the contents*/
    else {
      printOutput("%s is in source directory: now checking...\n", srcItem->name);
    }
    
    /* Now if we copied the subdirectory, or if it already existed, 
     * call dirsync to ensure that the contents will be identical.
     * Don't try to do this if we blocked the directory from being 
     * created -- only errors will result.
     */
    if(!unsafe) {
      dirsync(destpath,srcpath);
      copyStat(destpath, &srcItem->itemStat); // dirsync will change times -- need to reset them
    }
    
  }
}
/******************************************************************
 * dirsync takes two directories dir1 and dir2, and attempts to sync
 * them. Any files present in one but not the other will be copied 
 * appropriately, as will any subdirectories, unless an attempt is
 * made to copy a directory to itself or to a subdirectory of itself.
 * Symbolic links will not be followed but will be copied, although the 
 * modification times for symlinks will not be identical across directories.
 */

static int dirsync(char *src, char *dest) {
  
  Directory *srcDir = calloc(1,sizeof(Directory));
  Directory *destDir = calloc(1,sizeof(Directory));
  
  printOutput("\nNow syncing from %s to %s\n\n", src, dest);
  
  //first make filelists
  if(makeDirectory(src, srcDir)) {
    printError("makeDirectory", src);
  }
  
  if(makeDirectory(dest, destDir)) {
    printError("makeDirectory", src);
  }
  
  //move files from dest to src
  moveNeededFiles(destDir, dest, srcDir, src);
  
  // move files from src to dest
  moveNeededFiles(srcDir, src, destDir, dest);
  
  //move dirs from dest to src
  moveNeededDirs(destDir, dest, srcDir, src);
  
  // move dirs from src to dest
  moveNeededDirs(srcDir, src, destDir, dest);
  
  freeDir(srcDir);
  freeDir(destDir);
  
  return 0; 
  
}

int main(int argc, char *argv[]) {
  int c;
  int help = 0;
  
  while((c=getopt (argc, argv, "ho")) != -1) {
    switch(c) {
      case 'h':
	help = 1;
	break;
      case 'o':
	printoutput = 1;
	break;
      default:
	exit(1);
      
    }
  }
  
  if(help) {
    printf("Usage: dirsync [OPTIONS] [directory1] [directory2]\nPossible options are:\n\t-h: Print this message\n\t-o: Print output of program operation to stdout (can be redirected to file)\n");
    exit(0);
  }
  
  char *dir1, *dir2;
  
  if(optind + 2 == argc) {
    dir1 = argv[optind];
    dir2 = argv[optind+1];
  } else {
    fprintf(stderr, "Need one source and one destination directory!\nRun dirsync -h for more help.\n");
    exit(1);
  }
  
  DIR *dir_ptr;
  
  if((dir_ptr = opendir(dir1)) == NULL) {
    printError("opendir", dir1);
    printf("Run dirsync -h for more help.\n");
    return -1;
  } else {
    closedir(dir_ptr);
  }
  
  if((dir_ptr = opendir(dir2)) == NULL) {
    printError("opendir", dir2);
    printf("Run dirsync -h for more help.\n");
    return -1;
  } else {
    closedir(dir_ptr);
  }
  
  setPathMax();
  
  dirsync(dir1,dir2);
  return 0;
}

