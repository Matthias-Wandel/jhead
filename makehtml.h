// Include file for my HTML directory generator program.
// Matthias Wandel Dec 1999-April 2000

typedef struct tm timestruc;

#define STORE_FILEDATE 1
//-------------------------------------------------------------------
// Structure for a named directory entry.
typedef struct {
    char   Name[260];

    // File specific:
    int    Size;        // File size
    int    HasThumbnail;
    int    Referenced;
    unsigned short Width, Height; // Size of original.
    timestruc Date;
    #ifdef STORE_FILEDATE
        time_t FileDate;
    #endif
    

    // Dir specific:
    int    DoNotDescend;
    void * Subdir; // Points at subdirectory data (if the element is for a directory)
}DirEntry;

//-------------------------------------------------------------------
// Structure for a variable length list.   Used for lists of files and subdirs.
typedef struct {
    unsigned NumEntries;
    unsigned NumAllocated;
    DirEntry * Entries;
}VarList;

//-------------------------------------------------------------------
// Structure for a subdirectory.  Sort of like an I-node for the directory.
typedef struct {
    // Statistics on this directory and descending directories:
    timestruc Oldest;
    timestruc Newest;
    int       NumSubImages;
    int       NumSubVideos;

    char      FullPath[300]; // Path to what is pointed to.
    char      IndexName[100]; // Name of the file pointed to.
    char      Theme[50];     // Name of theme for organizing themes.

    int       IsThemeDir;
    int       NoAutoGenerate;
    int       IsRootDir;

    VarList   Dirs;   // List of subdirectories.
    VarList   Images; // List of Image files in this directory.
    VarList   Videos; // List of video clips (.avi)
}Dir_t;



//-------------------------------------------------------------------
void MakeHtmlOutput(Dir_t * Dir);
void CreateIndexFiles(Dir_t * Dir);
//-------------------------------------------------------------------
time_t CollectDirectory(char * DirName, VarList * Files, char * Extensions[]);
//-------------------------------------------------------------------
void CollectDirectoryImages(char * PathName, VarList * Files);
void CollectDirectoryVideos(char * PathName, VarList * Files);
void ParseDate(timestruc * thisdate, char * Line);
//-------------------------------------------------------------------
void LinkThemes(Dir_t * Root);
//-------------------------------------------------------------------
int  UpdateCache(char * Path, int ForceRefresh);
//-------------------------------------------------------------------
int  AddToList(VarList * List, DirEntry * Item);
void RemoveFromList(VarList * List, unsigned int Item);
void TrimList(VarList * List);
void ClearList(VarList * List);
int  FindName(VarList * List, char * Name);
void CopyFile(char * DestPath, char * SrcPath);
int CompareDates(const timestruc * t1, const timestruc * t2);
int CompareDatesHi(const timestruc * t1, const timestruc * t2);
void CombinePaths(char * Dest, char * p1, char * p2);
void RelPath(char * Rel, char * From, char * To);
int ExtCheck(char * Ext, char * Pattern);
//-------------------------------------------------------------------

extern char smallname[];
extern char CachefileName[];
extern int MinThumbnailWorthySize;
#ifdef _WIN32
    #define stat _stat
#endif

typedef int BOOL;

#define TRUE 1
#define FALSE 0

#ifdef WINDOWS
    #define SLASH_CHAR '\\'
    #define SLASH_STRING "\\"
#else
    #define SLASH_CHAR '/'
    #define SLASH_STRING "/"
#endif



