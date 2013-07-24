#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "dirsynctypes.h"


fileList *makeList() {	
  fileList *returnptr = calloc(1, sizeof(fileList));
  returnptr->dataStart = NULL;
  returnptr->len = 0;
  returnptr->reservedSpace = 0;
  return returnptr;
}

/******************************************************************
 * checkSize ensures that adding another item to flist will not push the 
 * array beyond its allocated space, or double the allocated space 
 * to accommodate new items.
 */
static void checkSize(fileList *flist) {
  
  /*check that the addition of an object will not exceed the current reservedSpace */
  if( (flist->len + 1) > flist->reservedSpace) {
    unsigned int newsize = flist->reservedSpace * 2; //if it does, double the reservedSpace
    
    /*ensure the the new reservedSpace is at least the minimum size (this only really matters when we add the first item */
    flist->reservedSpace = (newsize > MIN_FILELIST_SIZE) ? newsize : MIN_FILELIST_SIZE;
    flist->dataStart = realloc(flist->dataStart, flist->reservedSpace * sizeof(fileItem *));
  }
}

int itemComp(const void *i1, const void *i2) {
  fileItem * item1 = *(fileItem **)i1;
  fileItem * item2 = *(fileItem **)i2;
  return strcmp(item1->name, item2->name);
}

void addFile(fileList *flist, char *name, struct stat * itemstat) {
  if(!flist) {
    //print error
    return;
  }
  
  checkSize(flist);
  
  fileItem *file = calloc(1, sizeof(fileItem));
  
  int namelen = strlen(name)+1; //null byte
  char* filename = calloc(namelen, sizeof(char));
  
  filename = strcpy(filename,name);
  file->name = filename;
  
  if(itemstat != NULL) {
    file->itemStat = *itemstat;
  }
  
  flist->dataStart[flist->len] = file;
  flist->len++;
  
  qsort(flist->dataStart, flist->len, sizeof(flist->dataStart[0]), itemComp); // sort the list after adding a new item
  
}

fileItem *itemFind(fileList *list, fileItem *toFind) {
  if(list == NULL || toFind == NULL) {
    return NULL;
  }
  
  //use bsearch to find an item with the correct name
  fileItem **found = (fileItem **)bsearch(&toFind, list->dataStart, list->len, sizeof(list->dataStart[0]), itemComp);
  if(found == NULL) {
    return NULL; // it wasn't found
  }
  else
    return *found;
}

void freeFileList(fileList *tofree) {
  if(tofree == NULL) {
    return;
  }
  
  fileItem *p;
  int i;
  
  for(i=0; i < tofree->len; i++) {
    p = tofree->dataStart[i];
    free(p->name); //must free each name
    free(p); //as well as each pointer
  }
  
  free(tofree->dataStart); //now free the array
  free(tofree); //finally, free the fileList
}

void freeDir(Directory *tofree) {
  freeFileList(tofree->files);
  freeFileList(tofree->subdirs);
  free(tofree);
}

