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

VarList AllDirs;
Dir_t AllThemesDir;
char RootPath[300];

//----------------------------------------------------------------------------------
// Go through the built up data in memory and create directories.
//----------------------------------------------------------------------------------
static void MakeLinearList(Dir_t * Dir)
{
    int n;
    int a;
    DirEntry * Entries;
    DirEntry NewItem;
    //printf("Dir: %s\n",Dir->FullPath);

    if (Dir == NULL){
        printf("Internal error: Null directory\n");
        return;
    }


    memset(&NewItem, 0, sizeof(NewItem));
    NewItem.Subdir = Dir;
    strcpy(NewItem.Name, Dir->Theme);
    AddToList(&AllDirs, &NewItem);


    // Do the subdirectories.
    n = Dir->Dirs.NumEntries;

    //printf("%d subentries for %s\n",n,Dir->Theme);
    Entries = Dir->Dirs.Entries;
    for (a=0;a<n;a++){
        if (!Entries[a].DoNotDescend){
            MakeLinearList(Entries[a].Subdir);
        }
    }

}

//----------------------------------------------------------------------------------
// Comapre theme names for sorting.
//----------------------------------------------------------------------------------
static int SortCompareNames(const void * el1,  const void * el2)
{
    return strcmp(((DirEntry *)el1)->Name, ((DirEntry *)el2)->Name);
}


//----------------------------------------------------------------------------------
// Make a listing of all the themes of a particular name, wherever that may be.
//----------------------------------------------------------------------------------
static void CollectTheme(DirEntry * Theme, int NumSame)
{
    int a;
    Dir_t * ThemeDir;
    DirEntry ThisTheme;

    printf("Collecting theme: '%s'\n",Theme[0].Name);

    ThemeDir = malloc(sizeof(Dir_t));

    memset(ThemeDir, 0, sizeof(Dir_t));
    ThemeDir->Oldest.tm_year = 2037;
    ThemeDir->Newest.tm_year = 0;
    ThemeDir->IsThemeDir = 1;

    strcpy(ThemeDir->FullPath, RootPath);
    sprintf(ThemeDir->IndexName, "%s.html",Theme[0].Name);
    strcpy(ThemeDir->Theme, Theme[0].Name);

    memset(&ThisTheme, 0, sizeof(ThisTheme));
    strcpy(ThisTheme.Name, Theme[0].Name);
    ThisTheme.Subdir = ThemeDir;

    // Add it to the main list of themes.
    AddToList(&AllThemesDir.Dirs, &ThisTheme);


    strcpy(ThisTheme.Name, "Directories similar to this");

    // Other links shall not descend on creating output.
    ThisTheme.DoNotDescend = 1;

    //printf("Adding theme list to dirs\n");
    // Add the theme directory to each directory of that theme.
    for (a=0;a<NumSame;a++){
        Dir_t * ContentDir;
        ContentDir = Theme[a].Subdir;
        AddToList(&ContentDir->Dirs, &ThisTheme);
    }


    //printf("Adding dirs to theme list\n");
    // Add each of the similar directories to the themes directory.
    for (a=0;a<NumSame;a++){
        Dir_t * ContentDir;
        ContentDir = Theme[a].Subdir;
        strcpy(ThisTheme.Name, ContentDir->FullPath);
        ThisTheme.Subdir = ContentDir;

        AddToList(&ThemeDir->Dirs, &ThisTheme);
    }


}


//----------------------------------------------------------------------------------
// Cause similar directories to become linked.
//----------------------------------------------------------------------------------
void LinkThemes(Dir_t * Root)
{
    unsigned int a,b;
    int NumThemes;

    printf("Collecting all dirs\n");
    memset(&AllDirs, 0, sizeof(AllDirs));


    memset(&AllThemesDir, 0, sizeof(AllThemesDir));
    strcpy(AllThemesDir.FullPath, Root->FullPath);
    strcpy(AllThemesDir.IndexName, "Related-dirs.html");
    strcpy(AllThemesDir.Theme, "Related-dirs");
    AllThemesDir.IsThemeDir = 1;

    strcpy(RootPath, Root->FullPath);

    // Collect all directory theme names into a linear list.
    MakeLinearList(Root);

    printf("Have %d dirs\n",AllDirs.NumEntries);

    // Sort the list of themes to get same ones in order.    
    qsort( AllDirs.Entries, AllDirs.NumEntries, sizeof(DirEntry), SortCompareNames);

    NumThemes = 0;
    // Find repeating theme names.  If 2 or more same found, process them.
    for (a=0;a<AllDirs.NumEntries-1;){
        for (b=a+1;b<AllDirs.NumEntries;b++){
            if (strcmp(AllDirs.Entries[a].Name, AllDirs.Entries[b].Name)){
                break;
            }
        }
        if (b > a+1){
            CollectTheme(AllDirs.Entries+a, b-a);
            NumThemes += 1;
        }
        a = b;
    }

    if (NumThemes > 0){
        DirEntry ThisTheme;

        memset(&ThisTheme, 0, sizeof(ThisTheme));
        strcpy(ThisTheme.Name, "Directories with similar names");
        ThisTheme.Subdir = &AllThemesDir;

        AddToList(&Root->Dirs, &ThisTheme);
    }


    //ClearList(&AllDirs);
}


/*
Themes linking algorithm:


Running through list, if theme repeats, create theme directory.
Add theme directory to each Dir_t pointed to, as well as root.
Add pointed to directories to the theme directory.


For creating the HTML file:
    - Figure out how to make it not called index.html
    - Somehow specify NOT to descend subdirectories.

*/

