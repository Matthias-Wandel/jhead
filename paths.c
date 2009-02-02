//--------------------------------------------------------------------------------
// Module to do path manipulation for file moving of jhead.
//
// Matthias Wandel Feb 2 2009
//--------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <ctype.h>
#include <sys/stat.h>
#include "jhead.h"

#ifdef _WIN32
    #define SLASH '\\'
#else
    #define SLASH '/'
#endif
//--------------------------------------------------------------------------------
// Ensure that a path exists
//--------------------------------------------------------------------------------
int EnsurePathExists(const char * FileName)
{
    char NewPath[PATH_MAX*2];
    int a;
    int LastSlash = 0;

    // Extract the path component of the file name.
    strcpy(NewPath, FileName);
    a = strlen(NewPath);
    for (;--a;){
        if (NewPath[a] == SLASH){
            struct stat dummy;
            NewPath[a] = 0;
            printf("Try '%s'\n",NewPath);
            if (stat(NewPath, &dummy) == 0){
                if (LastSlash == 0){
                    // Full path exists.  No need to create any directories.
                    return 1;
                }else{
                    // Break out of loop, and go forward along path making
                    // the directories.
                    break;
                }
            }
            if (LastSlash == 0) LastSlash = a;
        }
    }

    // Now work forward.
    printf("Existing path: '%s'\n",NewPath);
    for(;FileName[a];a++){
        if (FileName[a] == SLASH){
            if (a == LastSlash) break;
            NewPath[a] = SLASH;
            printf("make dir '%s'\n",NewPath);
            if (mkdir(NewPath)){
                // Failed to create directory.
                return 0;
            }
        }
    }
    return 1;
}

//--------------------------------------------------------------------------------
// Flip slashes to native OS representation (for Windows)
//--------------------------------------------------------------------------------
#ifdef _WIN32
void SlashToNative(char * Path)
{
    int a;
    for (a=0;Path[a];a++){
        if (Path[a] == '/') Path[a] = SLASH;
    }
}
#endif

//--------------------------------------------------------------------------------
// Make a new path out of a base path, and a filename.
// Basepath is the base path, and FilePath is a filename, or path + filename.
//--------------------------------------------------------------------------------
void CatPath(char * BasePath, const char * FilePath)
{
    int l;
    l = strlen(BasePath);

    if (FilePath[1] == ':'){
        // Its a windows absolute path.
        l = 0;
    }

    if (FilePath[0] == SLASH || l == 0){
abs_path:
        // Its an absolute path, or there was no base path.
        strcpy(BasePath, FilePath);
        return;
    }
    
    if (BasePath[l-1] != SLASH){
        BasePath[l++] = SLASH;
        BasePath[l] = 0;
    }

    strcat(BasePath, FilePath);

    // Note that the combined path may contains things like "foo/../bar".   We assume
    // that the filesystem will take care of these.
}


/*
char Path[] = "test/a/b/../cdir/foo.jpg";

char BasePath[100];

main()
{
    SlashToNative(Path);
    EnsurePathExists(Path);

    CatPath(BasePath, "hello.txt");
    CatPath(BasePath, "world\\hello.txt");
    CatPath(BasePath, "\\hello.txt");
    CatPath(BasePath, "c:\\hello.txt");
    CatPath(BasePath, "c:\\world\\hello.txt");
    CatPath(BasePath, "c:\\abresl\\hello.txt");
   
}
*/