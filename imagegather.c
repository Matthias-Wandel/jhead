//----------------------------------------------------------------------------------
// Collects info about all jpegs in a particular directory.
// Uses my 'jhead' program to collect all the information and parses its output.
//----------------------------------------------------------------------------------

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "makehtml.h"


//----------------------------------------------------------------------------------
// Parse date in whatever format it may be in.
//----------------------------------------------------------------------------------
void ParseDate(timestruc * thisdate, char * Line)
{
    int fc;
    int a;
    timestruc t;

    t.tm_wday = -1;

    if (strstr(Line, "unknown")){
        // Date is completely unknown.
        t.tm_year = 0;
        t.tm_mon = -1;
        t.tm_mday = -1;
        *thisdate = t;
        return;
    }

    // Check for format: YYYY:MM:DD HH:MM:SS format.
    a = sscanf(Line, "%d:%d:%d %d:%d:%d",
            &t.tm_year, &t.tm_mon, &t.tm_mday,
            &t.tm_hour, &t.tm_min, &t.tm_sec);
        
    if (a == 6 || a == 3){
        // Its in YYYY:MM:DD HH:MM:SS format.
        if (t.tm_year < 1900 || t.tm_year > 2036){
            // Maybe not that foramt.  No idea what.
            goto error;
        }
        t.tm_mon -= 1;
        t.tm_year -= 1900;

        if (a == 3){
            // Only 3 number scanned.  Assume those mean date.
            t.tm_hour = -1;
            t.tm_min = -1;
            t.tm_sec = -1;
        }

        *thisdate = t;
        return;
    }
    t.tm_hour = t.tm_min = t.tm_sec = -1;

    if ((a = sscanf(Line,"%d/%d/%d",
        &t.tm_year, &t.tm_mon, &t.tm_mday)) >= 2){
        // This is a date as in markus's descript files.
        if (t.tm_year < 1900 || t.tm_year > 2036){
            // Maybe not that foramt.  No idea what.
            goto error;
        }

        t.tm_mon -= 1;
        t.tm_year -= 1900;

        if (a == 2){
            // Its YYYY/MM (no month day known - markus style).
            t.tm_mday = -1;
        }

        *thisdate = t;
        return;
    }

    t.tm_year = 0;
    t.tm_mon = t.tm_mday = -1;

    //It contains time and date.
    for(fc=0;;fc++){
        if (Line[fc] < 32) break;

        if (Line[fc] != ' ' && (Line[fc-1] == ' ' || fc == 0)){
            if (sscanf(Line+fc,"%*d:%*d:%*d") == 3){
                // This is a time descriptor.
                printf("time string\n");
                goto error;
            }

            if (sscanf(Line+fc,"%*d/%*d/%*d") == 3){
                // This is a date as in markus's descript files.
                printf("markus date\n");
                goto error;
            }

            if (Line[fc] >= '0' && Line[fc] <= '9'){
                int num;
                if (sscanf(Line+fc, "%d", &num) <= 0){
                    printf("number scan failure\n");
error:              printf("on line:'%s'",Line);      
                    t.tm_year = 0;
                    t.tm_mon = t.tm_mday = -1;
                    *thisdate = t;
                    return;
                    exit(-1);
                }
                if (num >= 1900 && num <= 2036){
                    t.tm_year = num-1900;
                    continue;
                }
                if (num >= 1 && num <= 31){
                    t.tm_mday = num;
                    continue;
                }
                printf("number out of range\n");
                goto error;
            }else{
                static char Months[][4] = {"jan","feb","mar","apr","may","jun",
                                           "jul","aug","sep","oct","nov","dec",
                                           "app" // for "approx"
                                          };

                for (a=0;a<13;a++){
                    if (memcmp(Line+fc,Months[a],3) == 0){
                        break;
                    } 
                }

                if (a >= 12){
                    if (a > 12){
                        printf("invalid month string '%s'\n",Line+fc);
                        goto error;
                    }
                }else{
                    t.tm_mon = a;
                }
            }
        }
    }

    *thisdate = t;
}


//----------------------------------------------------------------------------------
// Parse jhead output.
//----------------------------------------------------------------------------------
void ParseJheadOutput(VarList * Files, FILE * DirList)
{
    DirEntry ThisFile;    
    char LineBuffer[500];
    struct tm t;
    int a;

    memset(&ThisFile, 0, sizeof(ThisFile));

    memset(&t, 0, sizeof(t));

    // Default is unknown.
    t.tm_year = 0;
    t.tm_mon = t.tm_mday = -1;
    t.tm_hour = t.tm_min = t.tm_sec = -1;

    for(;;){
        if (fgets(LineBuffer, sizeof(LineBuffer), DirList) == NULL) break;

        if (memcmp(LineBuffer, "File name    :",14) == 0){
            int LastSlash = 14;

            // if an image already collected, store it and clear structure.
            if (ThisFile.Name[0]){
                AddToList(Files, &ThisFile);
            }
            // Clear out existing data.
            memset(&ThisFile, 0, sizeof(ThisFile));
            memset(&t, 0, sizeof(t));

            // Truncate carriage return.
            for (a=15;a<300;a++){
                if (LineBuffer[a] == '\\' || LineBuffer[a] == '/') LastSlash = a;
                if (LineBuffer[a] <= 13){
                    LineBuffer[a] = 0;
                    break;
                }
            }
            //printf("Name='%s'\n",LineBuffer+LastSlash+1);
            strcpy(ThisFile.Name, LineBuffer+LastSlash+1);
        }

        if (memcmp(LineBuffer, "File size    :",14) == 0){
            sscanf(LineBuffer+15,"%d",&ThisFile.Size);
        }

        // Does image have a thumbnail?
        // We are guessing here.  Could read the subdirectory but that takes more time.
        if (ThisFile.Size >= MinThumbnailWorthySize){
            ThisFile.HasThumbnail = 1;
        }else{
            ThisFile.HasThumbnail = 0;
        }

        if (memcmp(LineBuffer, "Date/Time    :",14) == 0){
            sscanf(LineBuffer+15,"%d:%d:%d %d:%d:%d",
                    &t.tm_year, &t.tm_mon, &t.tm_mday,
                    &t.tm_hour, &t.tm_min, &t.tm_sec);

            if (t.tm_year){
                // If the date is all zeros, probably wasn't set, so
                // don't use it (hopefully have another source of reasonable date
                // such as the file date.

                t.tm_mon -= 1;
                t.tm_year -= 1900;

                ThisFile.Date = t;
            }
        }


#ifdef STORE_FILEDATE
        if (memcmp(LineBuffer, "File date    :",14) == 0){
            struct tm ft;
            memset(&ft, 0, sizeof(t));

            a = sscanf(LineBuffer+15,"%d:%d:%d %d:%d:%d",
                    &ft.tm_year, &ft.tm_mon, &ft.tm_mday,
                    &ft.tm_hour, &ft.tm_min, &ft.tm_sec);
            ft.tm_mon -= 1;
            ft.tm_year -= 1900;

            if (a == 6){
                ThisFile.FileDate = mktime(&ft); 
            }
        }
#endif


        if (memcmp(LineBuffer, "Resolution   :",14) == 0){
            int w,h;
            if (sscanf(LineBuffer+15,"%d x %d", &w, &h) == 2){
                ThisFile.Width = w;
                ThisFile.Height = h;
            }
        }

        
        if (memcmp(LineBuffer, "Comment      :",14) == 0){
            // Look for date in here.
            if (memcmp(LineBuffer, "Comment      : date",19) == 0){
                for (a=0;LineBuffer[a];a++){
                    if (LineBuffer[a] >= 'A' && LineBuffer[a] <= 'Z'){
                        LineBuffer[a] += ('a'-'A');
                    }
                }
                ParseDate(&t, LineBuffer+20);
                ThisFile.Date = t;
            }
        }
    }

    if (ThisFile.Name[0]){
        AddToList(Files, &ThisFile);
    }

    fclose(DirList);

}


//----------------------------------------------------------------------------------
// Prepare a list of all images and corresponding info in a directory.
// Read in from the cached file.
//----------------------------------------------------------------------------------
void CollectDirectoryImages(char * PathName, VarList * Files)
{
    FILE * DirList;
    char LineBuffer[300];

    // Assumes that the cache file is up to date already.
    // - mainline called UpdateCache();

    // Open the chached file.
    CombinePaths(LineBuffer, PathName, CachefileName);
    DirList = fopen(LineBuffer, "r");

    if (DirList == NULL){
        fprintf(stderr,"Error: could not read directory\n");
        return;
    }

    ParseJheadOutput(Files, DirList);
}


//----------------------------------------------------------------------------------
// Collect up information about videos in a directory.
//----------------------------------------------------------------------------------
void CollectDirectoryVideos(char * PathName, VarList * Files)
{
    unsigned a,b;
    char * VideoNames[] = {".avi",".AVI",NULL};

    CollectDirectory(PathName, Files, VideoNames);

    if (Files->NumEntries){
        // There is videos in this directory.
        FILE * ReadHandle;
        char CmdLine[500];
        VarList ExtraInfo;

        sprintf(CmdLine, "jhead \"%s/\"*.thm \"%s/\"*.THM", PathName, PathName);

        #ifdef _WIN32
            ReadHandle = _popen(CmdLine, "r");
        #else
            ReadHandle = popen(CmdLine, "r");
        #endif
        if (ReadHandle == NULL){
            printf("Error: %s\n", CmdLine);
            return;
        }
        memset(&ExtraInfo, 0, sizeof(ExtraInfo));
        ParseJheadOutput(&ExtraInfo, ReadHandle);

        // Now correlate them.                                 
        printf("jhead output has %d files\n",ExtraInfo.NumEntries);
        for (b=0;b<Files->NumEntries;b++){
            unsigned l;
            l = strlen(Files->Entries[b].Name);
            for (a=0;a<ExtraInfo.NumEntries;a++){
                if (l == strlen(ExtraInfo.Entries[a].Name) && l > 4){
                    if (!memcmp(Files->Entries[b].Name, ExtraInfo.Entries[a].Name, l-3)){
                        // This .thm file matches.
                        ExtraInfo.Entries[a].Referenced = TRUE;
                        Files->Entries[b].HasThumbnail = TRUE;
                        Files->Entries[b].Date = ExtraInfo.Entries[a].Date;
                        Files->Entries[b].Width = ExtraInfo.Entries[a].Width;
                        Files->Entries[b].Height = ExtraInfo.Entries[a].Height;
                    }
                }
            }
        }

        for (a=0;a<ExtraInfo.NumEntries;a++){
            if (!ExtraInfo.Entries[a].Referenced){
                printf("Error: %s has no corresponding .avi file\n",ExtraInfo.Entries[a].Name);
            }
        }

        ClearList(&ExtraInfo);
    }

}


