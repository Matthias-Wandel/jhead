//----------------------------------------------------------------------------------
// Program to print a directory listing with dates.
// If file is from olympus digital camera, dates are from the file ocntents.
// otherwise, dates are just the file date.
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "makehtml.h"
#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#endif

static int DoRecursive = FALSE;
int MinThumbnailWorthySize;

char smallname[] = "_small"; // Name for thumbnail subdirectories
static char ExtraInfo[] = "xtra-info.txt";

typedef struct {
    unsigned int NumPictures;
    unsigned int NumThumbnails;
    unsigned int ThumbnailsCreated;
    unsigned int ThumbnailsDeleted;
    unsigned int TotPicSizes;
}StatType;
static StatType Done;

static int ForceCacheRefresh;

static char * ImageExtensions[] = {"jpg","jpeg",NULL};
//----------------------------------------------------------------------------------
// Maintain list of thumbnail directories.
//----------------------------------------------------------------------------------
static void MaintainThumbnails(char * PathName, VarList * Pictures)
{
    VarList Thumbnails;
    char SmallDir[300];
    unsigned int a;
    int temp;
    int HaveThumbnails;

    memset(&Thumbnails, 0, sizeof(VarList));

    CombinePaths(SmallDir, PathName, smallname);

    // Make sure thumbnail directory exists.
    #ifdef _WIN32
        mkdir(SmallDir);
    #else
        mkdir(SmallDir,0777);
    #endif

    // Collect existing list of thumbnails.
    CollectDirectory(SmallDir, &Thumbnails, ImageExtensions);

    HaveThumbnails = FALSE;

    for (a=0;a<Pictures->NumEntries;a++){
        if (Pictures->Entries[a].Size < MinThumbnailWorthySize){
            // No thumbnail for this picture, or a [tag] (not a picture).
            Pictures->Entries[a].Referenced = FALSE;
        }else{
            Done.NumThumbnails += 1;
            Pictures->Entries[a].Referenced = TRUE;
            temp = FindName(&Thumbnails, Pictures->Entries[a].Name);
            if (temp >= 0){
                Thumbnails.Entries[temp].Referenced = 1;
            }else{
                // Copy picture for thumbnail
                char ExecString[600];
                char DestString[500];
                CombinePaths(DestString, PathName, smallname);
                CombinePaths(DestString, DestString, Pictures->Entries[a].Name);

                CombinePaths(ExecString, PathName, Pictures->Entries[a].Name);
                CopyFile(DestString, ExecString);

                printf("Making Thumbnail: \"%s\"\n",Pictures->Entries[a].Name);

                // Next mogrify the picture.
                sprintf(ExecString, "mogrify -geometry 1600x240 \"%s\"",DestString);
                system(ExecString);
                Done.ThumbnailsCreated += 1;
            }
            HaveThumbnails = TRUE;
        }
    }

    // Remove any unreferenced thumbnails.
    for (a=0;a<Thumbnails.NumEntries;a++){

        if (Thumbnails.Entries[a].Referenced == 0){
            char RemoveName[600];
            CombinePaths(RemoveName, SmallDir, Thumbnails.Entries[a].Name);
            printf("Deleting \"%s\"\n",RemoveName);
            unlink(RemoveName);
            Done.ThumbnailsDeleted += 1;
        }
    }

    // Thumbnail directory _should_ be empty.  Remove it.
    if (HaveThumbnails == FALSE){
        rmdir(SmallDir);
    }

    ClearList (&Thumbnails);
}

//----------------------------------------------------------------------------------
// Extract a 'theme' name from the directory name for linking similar ones together.
//----------------------------------------------------------------------------------
static void ThemeName(char * Theme, char * PathName) 
{
    int LastSeg;
    int a;
    LastSeg = 0;

    // Identify last component of the path
    for (a=0;;a++){
        if (PathName[a] == '\\' || PathName[a] == '/') LastSeg = a+1;
        if (PathName[a] == '\0') break;
    }

    // Take the last path part, but not past a '-'.
    for (a=0;a<49;a++){
        if (PathName[LastSeg+a] == '-') break;
        if (PathName[LastSeg+a] == '\0') break;
        Theme[a] = PathName[LastSeg+a];
    }
    Theme[a] = '\0';

    // Trim trailing spaces.
    for(;;){
        a--;
        if (a <=0) break;
        if (Theme[a] != ' ') break;
        Theme[a] = '\0';
    }
    //printf("Path  : %s\n",PathName);
    //printf("Themne: %s\n",Theme);
}



//----------------------------------------------------------------------------------
// Unpad a string for parsing xtra-info.txt files.
//----------------------------------------------------------------------------------
static char * UnpadString(char * String)
{
    int a;
    while(*String == ' ' || *String == '\t'){
        String++;
    }

    if (*String == '\0') return NULL;

    a = strlen(String);

    while (a > 0 && ( String[a-1] == '\t' || String[a-1] == ' ' 
                   || String[a-1] == '\r' || String[a-1] == '\n')) a--;
    String[a] = '\0';

    //printf("Arg='%s'\n",String);
    return String;
}

//----------------------------------------------------------------------------------
// Check for special instructions in a directory.
//----------------------------------------------------------------------------------
static void ReadXtraInfo(Dir_t * Dir, char * PathName)
{
    char FileName[400];
    char LineBuffer[100];
    FILE * XtraFile;

    CombinePaths(FileName, PathName, ExtraInfo);

    XtraFile = fopen(FileName, "r");
    if (XtraFile == NULL) return;

    for(;;){
        char * Arg;
        char * Cmd;
        if (fgets(LineBuffer, sizeof(LineBuffer), XtraFile) == NULL) break;
        if (LineBuffer[0] == ';') continue; // Comment.

        // Cut off trailing comment.        
        if (Arg = strstr(LineBuffer, ";")) *Arg = 0;

        Arg = strstr(LineBuffer, "=");
        if (Arg == NULL) goto bad_line;
        *Arg = '\0';

        Cmd = UnpadString(LineBuffer);
        Arg = UnpadString(Arg+1);
        if (Cmd == NULL || Arg == NULL) goto bad_line;
        printf("Cmd='%s' Arg='%s'\n",Cmd,Arg);
        if (strcmp(Cmd, "index") == 0){
            // Specified file ovverides auto-generated index for this directory.
            strcpy(Dir->IndexName,Arg);
            Dir->NoAutoGenerate = TRUE;
        }else if (strcmp(Cmd, "link") == 0){
            // Add a link to a spcified target.
            Dir_t * LinkInfo;
            DirEntry LinkEntry;

            // This is a link to another piece of conent.
            // Create a directory structure so that the directory entry for this
            // directory has something to point at.
            LinkInfo = (Dir_t *) malloc(sizeof(Dir_t));
            memset(LinkInfo, 0, sizeof(Dir_t));

            LinkInfo->Oldest.tm_year = 2037; // (after the end of time_t)
            LinkInfo->Newest.tm_year = 0;    // Indicates unknown.
            
            strcpy(LinkInfo->FullPath, PathName);
            strcpy(LinkInfo->IndexName, Arg);

            // Now create an entry for the directory list, which points at the pseoudo directory struct.
            memset(&LinkEntry, 0, sizeof(DirEntry));
            strcpy(LinkEntry.Name, Arg);
            LinkEntry.DoNotDescend = TRUE;      // Do not descend on tree scans.
            LinkEntry.Subdir = LinkInfo;

            AddToList(&Dir->Dirs, &LinkEntry);
        }else if (strcmp(Cmd, "date") == 0){
            timestruc NewDate;
            ParseDate(&NewDate, Arg);

            if (CompareDates(&Dir->Oldest, &NewDate) > 0){
                Dir->Oldest = NewDate;
            }
            if (CompareDatesHi(&Dir->Newest, &NewDate) < 0){
                Dir->Newest = NewDate;
            }
        }else{
            printf("Command '%s' not undestood\n", Cmd);
            exit(-1);
        }
    }
    
    if(0){
        bad_line:
        printf("File '%s': Bad line:\n    %s\n",FileName, LineBuffer);
        exit(-1);
    }


    fclose(XtraFile);
}



//----------------------------------------------------------------------------------
// Process one directory.  Returns pointer to summary.
//----------------------------------------------------------------------------------
Dir_t * CollectImageTree(char * PathName)
{
    Dir_t * Dir;
    VarList * Subdirs;
    VarList * Images;
    VarList * Videos;
    unsigned int a;
    int MaintainThumbs;

    Dir = (Dir_t *) malloc(sizeof(Dir_t));
    memset(Dir, 0, sizeof(Dir_t));

    if (Dir == NULL){
        printf("out of memory\n");
        // Very bad.  Don't attempt to fix it.
        exit(-1);
    }

    Subdirs = &Dir->Dirs;
    Images = &Dir->Images;
    Videos = &Dir->Videos;

    printf("Dir: '%s'\n",PathName);
    if (strlen(PathName) > 250){
        printf("Path string is too long\n");
        exit(0);
    }

    Dir->Oldest.tm_year = 2037; // (after the end of time_t)
    Dir->Newest.tm_year = 0;    // Indicates unknown.

    strcpy(Dir->FullPath, PathName);
    strcpy(Dir->IndexName, "index.html");
    ThemeName(Dir->Theme, PathName);

    // Update cache and check wether thumbnails need regenerating.
    MaintainThumbs = UpdateCache(PathName, ForceCacheRefresh);

    ReadXtraInfo(Dir, PathName);

    if (Dir->NoAutoGenerate){
        // This is a manually created index.  Just leave it alone.
        return Dir;
    }

    // Collect stuff from this directory.
    if (DoRecursive){
        CollectDirectory(PathName, Subdirs, NULL);
    }

    CollectDirectoryImages(PathName, Images);

    TrimList(Images); // Unallocate spare space.
    TrimList(Subdirs); // Unallocate spare space.

    // Update statistics for what is in the database.
    for (a=0;a<Images->NumEntries;a++){
        Done.NumPictures += 1;
        Done.TotPicSizes += Images->Entries[a].Size;
    }

    if (MaintainThumbs){
        // The cached file was out of date.  
        // Thumbnails may need updating.
        MaintainThumbnails(PathName, Images);
    }

    // Now collect all the videos.
    CollectDirectoryVideos(PathName, Videos);


    // Now do all subdirs (if so ordered)
    if (DoRecursive){
        for (a=0;a<Subdirs->NumEntries;a++){
            char NewPrefix[500];
            DirEntry ThisOne;
            Dir_t * temp;

            ThisOne = Subdirs->Entries[a];

            if (ThisOne.DoNotDescend) continue;

            CombinePaths(NewPrefix, PathName, ThisOne.Name);
            strcpy(Subdirs->Entries[a].Name, ThisOne.Name); // Fill in the name

            Subdirs->Entries[a].Subdir = temp = CollectImageTree(NewPrefix);

            if (temp->NumSubImages == 0 && temp->NumSubVideos == 0 && temp->NoAutoGenerate == FALSE){
                // This subdirectory contains nothing.  Remove from the list.
                RemoveFromList(Subdirs, a);
                a--; // Thigs have moved back.
            }else{
                Subdirs->Entries[a].Date = temp->Oldest;
            }
        }
    }
    

    //----------------------------------------------------------------------------
    // Compile statistics for this directory.
   
    Dir->NumSubImages = Images->NumEntries;
    Dir->NumSubVideos = Videos->NumEntries;

    for (a=0;a<Images->NumEntries;a++){
        if (Images->Entries[a].Date.tm_year > 0){
            // Pic has a known date.  Use in directory summary statistics.
            if (CompareDates(&Dir->Oldest, &Images->Entries[a].Date) > 0){
                Dir->Oldest = Images->Entries[a].Date;
            }
            if (CompareDatesHi(&Dir->Newest, &Images->Entries[a].Date) < 0){
                Dir->Newest = Images->Entries[a].Date;
            }
        }
    }


    // Add the subdirectory statistics to the staticstics for this directory.
    for (a=0;a<Subdirs->NumEntries;a++){
        Dir_t * temp;
        temp = Subdirs->Entries[a].Subdir;

        if (CompareDates(&Dir->Oldest, &temp->Oldest) > 0){
            Dir->Oldest = temp->Oldest;
        }
        if (CompareDatesHi(&Dir->Newest, &temp->Newest) < 0){
            Dir->Newest = temp->Newest;
        }
        Dir->NumSubImages += temp->NumSubImages;
        Dir->NumSubVideos += temp->NumSubVideos;
    }

    return Dir;
}

//----------------------------------------------------------------------------------
// Show valid command line options
//----------------------------------------------------------------------------------
void Usage(void)
{
    printf("Usage:\n"
           "getdates [-S] [root path]\n"
           "          -S    = recurse subdirectories\n"
           "          -i    = Invalidate all cached imagedata.cached files\n"
           "          -Tn   = No thumbnails\n"
           "          -T123 = Min thumbnail size 123k\n"
           "          root path = starting path\n"
           );

    exit(-1);
}


//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    int a;
    char * RootPath = "";

    MinThumbnailWorthySize = 60000;
    ForceCacheRefresh = FALSE;

    // Parse the command line options.
    for (a=1;a<argc;a++){
        if (argv[a][0] == '-'){
            if (strcmp(argv[a],"-S") == 0){
                // Recurse subdirectories.
                DoRecursive = TRUE;
            }else if (strcmp(argv[a],"-i") == 0){
                // Recurse subdirectories.
                ForceCacheRefresh = TRUE;
            }else if (argv[a][1] == 'T'){
                // Set minimum picture size for thumbnail.
                if (argv[a][2]=='n'){
                    // No thumbnails.
                    MinThumbnailWorthySize = 0x7fffffff;
                }else{
                    if (sscanf(argv[a],"-T%i",&MinThumbnailWorthySize) != 1){
                        goto badopt;
                    }
                    MinThumbnailWorthySize *= 1000;
                }
            }else{
                badopt:
                fprintf(stderr,"Error: Argument '%s' not understood\n",argv[a]);
                Usage();
            }
        }else{
            // Plain argument with no dashes means root path.
            RootPath = argv[a];
        }
    }

    {
        Dir_t * Root;

        // Run the information gathering & thumbnailing pass.
        Root = CollectImageTree(RootPath);
        Root->IsRootDir = 1;

        if (DoRecursive){
            // Link related directiroes together
            LinkThemes(Root);
        }

        // Create the HTML output files.
        CreateIndexFiles(Root);
    }

    printf("Work summary:\n");
    printf("Total pictures: %d pic, %u bytes\n",Done.NumPictures, Done.TotPicSizes);
    printf("Thumbnails    : %d, %d new, %d deleted\n",Done.NumThumbnails, Done.ThumbnailsCreated, Done.ThumbnailsDeleted);

    return 0;
}

