//--------------------------------------------------------------------------
// Program to check if file with jpeg's information is up to date.
//
// Matthias Wandel, Mar 2000
//--------------------------------------------------------------------------
#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

#include "makehtml.h"

#ifdef _WIN32
    #include <process.h>
    #include <io.h>
#endif

static char * ImageExtensions[] = {"jpg","jpeg",NULL};
char CachefileName[] = "imagedata.cached";
//--------------------------------------------------------------------------
// Check if the cache file is up to date.
//--------------------------------------------------------------------------
static int CheckCacheOk(char * Path)
{
    char CacheName[200];
    FILE * CacheFile;
    int a;
    unsigned n;

    struct stat CacheStat;
    time_t NewestImageTime;

    VarList FileList;

    memset(&FileList, 0, sizeof(VarList));

    //---------------------------------------------------------
    // Chenck for presence and age of chached image data info.

    CombinePaths (CacheName, Path, CachefileName);


    if (stat(CacheName, &CacheStat)){
        // There probably is no cache.
        return FALSE;
    }

    //---------------------------------------------------------
    // Read the directory.

    NewestImageTime = CollectDirectory(Path, &FileList, ImageExtensions);

    if (NewestImageTime > CacheStat.st_mtime){
        // There is images newer than the imagedata.cahced file.
        return FALSE;
    }

    //---------------------------------------------------------
    // Now read the cached file and check the entries are also cached.

    CacheFile = fopen(CacheName, "r");
    if (CacheFile == NULL){
        fprintf(stderr, "Error! could not read cache file!");
        goto list_mismatch;
    }

    for (;;){
        char LineBuffer[300];
        if (fgets(LineBuffer, 300, CacheFile) == NULL) break;

        if (memcmp(LineBuffer, "File name    :",14) == 0){

            int LastSlash = 14;

            // Truncate carriage return.
            for (a=15;a<300;a++){
                if (LineBuffer[a] == '\\' || LineBuffer[a] == '/') LastSlash = a;
                if (LineBuffer[a] <= 13){
                    LineBuffer[a] = 0;
                    break;
                }
            }

            a = FindName(&FileList, LineBuffer+LastSlash+1);
            if (a < 0){
                // The cached list has a file that the directory didn't have.
                printf("file '%s' not in directory\n",LineBuffer+15);                
                goto list_mismatch;
            }else{
                // Mark file as also in the directory.
                FileList.Entries[a].Referenced = 1;
            }
        }
    }

    //---------------------------------------------------------
    // Now check that all the files in the directory were also in the cache.

    for (n=0;n<FileList.NumEntries;n++){
        if (FileList.Entries[n].Referenced == 0){
            printf("File '%s' not in cached\n",FileList.Entries[n].Name);
            goto list_mismatch;
        }
    }

    fclose(CacheFile);

    // Got this far.  Everything matches.

    ClearList(&FileList);
    return 1;

    //-----------Failed----------------

list_mismatch:
    fclose(CacheFile);
    ClearList(&FileList);
    return 0;
}
//--------------------------------------------------------------------------
// Regenerate the cache file.
//--------------------------------------------------------------------------
static void RegenCache(char * Path)
{
    FILE * Prog;
    FILE * Cache;
    char CommandString[500];
    char FileName[300];
    int a;

    CombinePaths(FileName, Path, CachefileName);
    Cache = fopen(FileName, "w");

    if (Cache == NULL){
        printf("could not open cache file\n");
        exit(-1);
    }

    #ifdef _WIN32
        CombinePaths(FileName, Path, "*.jpg");
        for (a=0;FileName[a];a++){
            // windows-ify path name.
            if (FileName[a] == '/') FileName[a] = '\\';
        }
        #define popen _popen

        sprintf(CommandString, "jhead \"%s\"",FileName);
    #else
        // Beware of quote semantics and wildcard expansion interaction!
        sprintf(CommandString, "jhead \"%s\"/*.jpg",Path[0] ? Path:".");
    #endif
    //printf("cmd: %s\n",CommandString);

    Prog = popen(CommandString, "r");

    if (Prog == NULL){
        printf("Cold not run the jhead program\n");
        fclose(Cache);
        exit(-1);
    }

    for(;;){
        char Trans[1000];
        unsigned n;
        n = fread(&Trans, 1, 1000, Prog);
        if (n <= 0) break; // End of file.
        if (fwrite(&Trans, 1, n, Cache) != n){
            printf("Cache write error!\n");
            exit(-1);
        }
    }
    fclose(Cache);

    #ifdef _WIN32
        _pclose(Prog);
    #else
        pclose(Prog);
    #endif
}

//--------------------------------------------------------------------------
// Regenerate the cache if necessary.  Returns TRUE if cache changed.
//--------------------------------------------------------------------------
int UpdateCache(char * Path, int ForceRefresh)
{
    if (ForceRefresh || !CheckCacheOk(Path)){
        // Update the cache.
        printf("Rescanning directory \"%s\"\n",Path);
        RegenCache(Path);
        return TRUE;
    }else{
        //printf("cache is good\n");
        return FALSE;
    }
}
