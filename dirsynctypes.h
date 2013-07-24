#define MIN_FILELIST_SIZE 4
#define printOutput(args ...) if (printoutput) fprintf(stdout, args)

/******************************************************************
 * A fileItem consists of two parts: a string to hold the name
 * of the file/directory, and a struct stat, itemStat, to 
 * hold information about the file (such as modification times 
 * and other information documented in the stat documentation.
 */

typedef struct fileItem {
  char *name;
  struct stat itemStat;
} fileItem;

/******************************************************************
 * A fileList is a list of fileItems. dataStart is a dynamically 
 * resizing array of fileItem pointers. len is used to indicate 
 * how many fileItems are in the current list, while reservedSpace 
 * keeps track of the size of the array so that it can be appropriately
 * resized when new items are added. The fileList is always sorted 
 * by fileItem name.
 */

typedef struct fileList {
  fileItem **dataStart;
  unsigned int len;
  unsigned int reservedSpace;
} fileList;

/******************************************************************
 * A Directory consists of a fileList of files and a fileList
 * of subdirectories.
 */

typedef struct Directory {
  fileList *files;
  fileList *subdirs;
} Directory;

/******************************************************************
 * makeList returns a pointer to a new fileList with no 
 * reservedSpace and no length.
 */
fileList *makeList();

/******************************************************************
 * addFile adds a new item to the fileList specified by flist, with
 * the given name and stat struct. If adding an item to the list 
 * requires expansion of the array, the array will be doubled in 
 * size. The fileList will be sorted after insertion of the 
 * new member.
 */
void addFile(fileList *flist, char *name, struct stat * itemstat);

/******************************************************************
 * itemComp compares the two fileItems pointed to by i1 and i1.
 * The comparison is made on the basis of filenames -- if 
 * i1->name and i2->name are the same, itemComp returns 0, 
 * otherwise, it returns non-zero.
 */
int itemComp(const void *i1, const void *i2);

/******************************************************************
 * itemFind attempts to find the item pointed to by toFind in the 
 * given list. If a fileItem with a matching name is found, a 
 * pointer to the item will be returned. Otherwise, NULL is returned.
 * Since each fileList is in sorted order, we can use bsearch to find the 
 * item.
 */
fileItem *itemFind(fileList *list, fileItem *toFind);

/******************************************************************
 * freeFileList frees the given fileList as well as its dataStart
 * array and its constituent pointers.
 */
void freeFileList(fileList *tofree);

/******************************************************************
 * freeDir frees the given Directory and the fileLists contained 
 * in the Directory.
 */
void freeDir(Directory *tofree);
