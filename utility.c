#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "makehtml.h"

//----------------------------------------------------------------------------------
// Add something to a VarList.
//----------------------------------------------------------------------------------
int AddToList(VarList * List, DirEntry * Item)
{
    if (List->Entries == NULL){
        DirEntry * Temp;
        Temp = (DirEntry *)malloc(sizeof(DirEntry) * 20);

    List->Entries = Temp;

        //List->Entries = (DirEntry *)malloc(sizeof(DirEntry) * 20);
        List->NumAllocated = 20;
        List->NumEntries = 0;

        if (List->Entries == NULL){
            printf("\nError: Malloc failure!\n");
            exit(-1);
        }
    }else{
        if ((List->NumEntries) >= List->NumAllocated){
            List->NumAllocated += List->NumAllocated/2;
            List->Entries = (DirEntry *)realloc(List->Entries, sizeof(DirEntry)*List->NumAllocated);

            if (List->Entries == NULL){
                printf("\nError: Realloc failure!\n");
                exit(-1);
            }
        }
    }

    List->Entries[List->NumEntries] = *Item;
    return List->NumEntries++;
}

//----------------------------------------------------------------------------------
// Delete an item from the list.
//----------------------------------------------------------------------------------
void RemoveFromList(VarList * List, unsigned int Item) 
{
    if (Item < 0 || Item >= List->NumEntries){
        printf("invalid remove index\n");
        exit(-1);
    }
    memcpy(List->Entries+Item, List->Entries+Item+1, sizeof(DirEntry) * (List->NumEntries-1-Item));
    List->NumEntries -= 1;
}

//----------------------------------------------------------------------------------
// Un-allocate extra allocated memory.
//----------------------------------------------------------------------------------
void TrimList(VarList * List)
{
    if (List->NumAllocated > List->NumEntries+2){
        // Leave a spare element - we still add the themes link after trimming.
        List->NumAllocated = List->NumEntries+1;
        List->Entries = (DirEntry *)realloc(List->Entries, sizeof(DirEntry)*List->NumAllocated);
    }
}

//----------------------------------------------------------------------------------
// Get rid of a VarList
//----------------------------------------------------------------------------------
void ClearList(VarList * List)
{
    free(List->Entries);
    List->NumEntries = 0;
    List->NumAllocated = 0;
}

//----------------------------------------------------------------------------------
// Find a name in VarList
//----------------------------------------------------------------------------------
int FindName(VarList * List, char * Name)
{
    unsigned int a;
    for (a=0;a<List->NumEntries;a++){
        if (strcmp(List->Entries[a].Name, Name) == 0){
            return a;
        }
    }
    // Name was not found in the list.
    return -1;
}
//----------------------------------------------------------------------------------
// Combine two paths into one.
//----------------------------------------------------------------------------------
void CombinePaths(char * Dest, char * p1, char * p2)
{
    int Lastp1char;
    strcpy(Dest, p1);
    Lastp1char = 0;
    if (p1[0]){
        Lastp1char = p1[strlen(p1)-1];
    }

    if (p1[0] != '\0'){
        if (Lastp1char != '\\' && Lastp1char != '/'){
            strcat(Dest, SLASH_STRING);
        }
    }
    strcat(Dest, p2);
}

//----------------------------------------------------------------------------------
// Compute how to get from 'From' path to 'To' path
//----------------------------------------------------------------------------------
void RelPath(char * Rel, char * From, char * To)
{
    int a;
    int FirstDifference;
    int WasSlash;
    int LenFrom, LenTo;

    FirstDifference = 0;
    Rel[0] = 0;

    // Starting from left, pop off all equal directory components.
    for (a=0;;a++){
        BOOL SepFrom, SepTo;
        SepFrom = (From[a] == '\0' || From[a] == '\\' || From[a] == '/');
        SepTo   = (To[a] == '\0' || To[a] == '\\' || To[a] == '/');

        if (SepFrom && SepTo){
            FirstDifference = a+1;
        }

        if (From[a] != To[a]){
            break;
        }

        if (From[a] == '\0'){
            // Paths are identical.
            return;
        }
    }

    //printf("first diff at %d\n",FirstDifference);

    // Now count how many directories we need to go up before going back down.
    // FirstDifference is index of first path segment that differs.

    // Note: This code not smart enough to understand '..' in the 'from' path.

    LenFrom = strlen(From);
    LenTo = strlen(To);

    WasSlash = 1;
    for (a=FirstDifference;;a++){
        if (a > LenFrom) break;
        if (From[a] == '\0') break;
        if (From[a] == '\\' || From[a] == '/'){
            WasSlash = 1;                                       
        }else{
            if (WasSlash){
                // Using forward slashes - Windows browsers understand this, and so
                // do Unitx browsers (backslash is windows only)
                strcat(Rel, "../");
                WasSlash = 0;
            }
        }
    } 
    if (LenTo > FirstDifference){
        strcat(Rel, To+FirstDifference);
    }

    /*
    printf("From:%s\n",From);
    printf("To  :%s\n",To  );
    printf("Rel :%s\n",Rel );
    */

}



//----------------------------------------------------------------------------------
// Copy a picture file (for image magic to manipulate it in place)
//----------------------------------------------------------------------------------
void CopyFile(char * DestPath, char * SrcPath)
{
    static unsigned char BigBuf[200000];
    FILE * Rd, * Wr;

    //printf("copy from:%s\n to:%s\n",SrcPath, DestPath);

    Rd = fopen(SrcPath, "rb");
    if (Rd == NULL){
        printf("could not read '%s'\n", SrcPath);
        exit(-1);
    }
    Wr = fopen(DestPath, "wb");
    if (Wr == NULL){
        printf("Could not write thumbnail file\n");
        exit(-1);
    }

    for(;;){
        int bytes;
        bytes = fread(BigBuf, 1, sizeof(BigBuf),Rd);
        if (bytes <= 0) break;
        fwrite(BigBuf, 1, bytes,Wr);
    }

    fclose(Rd);
    fclose(Wr);
}


//----------------------------------------------------------------------------------
// Compare time structures element by element.  
// Unknown elemets are treated as newest
//----------------------------------------------------------------------------------
int CompareDates(const timestruc * t1, const timestruc * t2)
{
    if (t1->tm_year != t2->tm_year) return t1->tm_year - t2->tm_year;
    if (t1->tm_mon != t2->tm_mon)   return t1->tm_mon - t2->tm_mon;
    if (t1->tm_mday != t2->tm_mday) return t1->tm_mday - t2->tm_mday;
    if (t1->tm_hour != t2->tm_hour) return t1->tm_hour - t2->tm_hour;
    if (t1->tm_min != t2->tm_min)   return t1->tm_min - t2->tm_min;
    if (t1->tm_sec != t2->tm_sec)   return t1->tm_sec - t2->tm_sec;
    return 0;
}



//----------------------------------------------------------------------------------
// Compare time structures element by element.  
// Unknown elemets are treated as oldest.
//----------------------------------------------------------------------------------
int CompareDatesHi(const timestruc * t1, const timestruc * t2)
{
    // Trick: And-ing wiht 0xffff turns negative numbers into large positive ones,
    // thus making 'unknown' large.

    if (t1->tm_year != t2->tm_year) return (t1->tm_year & 0xffff) - (t2->tm_year & 0xffff);
    if (t1->tm_mon != t2->tm_mon)   return (t1->tm_mon  & 0xffff) - (t2->tm_mon  & 0xffff);
    if (t1->tm_mday != t2->tm_mday) return (t1->tm_mday & 0xffff) - (t2->tm_mday & 0xffff);
    if (t1->tm_hour != t2->tm_hour) return (t1->tm_hour & 0xffff) - (t2->tm_hour & 0xffff);
    if (t1->tm_min != t2->tm_min)   return (t1->tm_min  & 0xffff) - (t2->tm_min  & 0xffff);
    if (t1->tm_sec != t2->tm_sec)   return (t1->tm_sec  & 0xffff) - (t2->tm_sec  & 0xffff);
    return 0;
}
