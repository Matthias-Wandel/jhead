//----------------------------------------------------------------------------------
// HTML output generator part of my HTML thumbnail tree building program.
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#endif

#include "makehtml.h"

extern void MyGlob(const char * Pattern , int DoSubdirsParm, void (*FileFuncParm)(const char * FileName));

//----------------------------------------------------------------------------------
// Make date (or partial date) strings if date is uncertain
//----------------------------------------------------------------------------------
static void MakeDateString(char * String, timestruc time)
{
    if (time.tm_year >= 30 && time.tm_year < 130){
        if (time.tm_mon >= 0){
            // Don't show exact dates prior to 1970.  Microsoft standard library
            // crashes on dates prior to 1970 with ctime.
            if (time.tm_mday >= 1 && time.tm_year >= 70){
                if (time.tm_mday < 1 || time.tm_mday > 31){
                    // Library crashes on invalid month dates, so pre-check.
                    printf("Error: Month day is out of whack! (%d)!",time.tm_mday);
                    exit(-1);
                }

                // Year month and date are known.
                time.tm_hour = 12;
                time.tm_min = 0;
                time.tm_sec = 0;
                mktime(&time);
                strftime(String, 30, "%a %b %d %Y",&time);
            }else{
                // Only year and month are known.
                static char Months[][4] = {"Jan","Feb","Mar","Apr","May","Jun",
                                           "Jul","Aug","Sep","Oct","Nov","Dec"};
                sprintf(String, "%s %d",Months[time.tm_mon], time.tm_year+1900);
            }
        }else{
            // Only the year is known.
            sprintf(String,"%d",time.tm_year+1900);
        }
    }else{
        String[0] = 0;
        //strcpy(String, "Unknown");
    }
}

//----------------------------------------------------------------------------------
// Escape filenames - so it works on Unix also.
//----------------------------------------------------------------------------------
static char * EscapePath(char * Path)
{
    static char OutputPath[1000];
    int a;
    int OutIndex;
    OutIndex = 0;
    for (a=0;a<500;a++){
        switch(Path[a]){
            default:
                OutputPath[OutIndex++] = Path[a];
                break;

            case ' ':
            case '&':
            case '%':
                // There's probably other unsafe characters - will add these as I encounter them!
                sprintf(OutputPath+OutIndex,"%%%02x",Path[a]);
                OutIndex+=3;
                break;
                
            case 0:
                goto done;
        }
        if (OutIndex > 500){
            printf("Path error!");
            exit(-1);
        }
    }
done:
    OutputPath[OutIndex] = 0;
    return OutputPath;
}

//----------------------------------------------------------------------------------
// Make a subdirectory reference in the HTML file.
//----------------------------------------------------------------------------------
static void MakeDirEntry(FILE * HtmlFile, Dir_t * ThisDir, DirEntry * Subdir)
{
    char Oldest[30], Newest[30];
    Dir_t * LinkTo;
    char LinkPath1[300];
    char LinkPath2[300];

    LinkTo = Subdir->Subdir;

    //    if (Subdir->Name[0] == 0) return; // Stamped out name.

    MakeDateString(Oldest, LinkTo->Oldest);
    MakeDateString(Newest, LinkTo->Newest);

    //printf("This dir:%s\n",ThisDir->FullPath);
    //printf("Point to:%s\n",LinkTo->FullPath);

    // Figure out how to get there from here.  The directory linked
    // is not necessarily a subdirectory.
    CombinePaths(LinkPath1, LinkTo->FullPath, LinkTo->IndexName);
    
    RelPath(LinkPath2, ThisDir->FullPath, LinkPath1);
                                   
    fprintf(HtmlFile, "<a href=\"%s\">",EscapePath(LinkPath2));

    fprintf(HtmlFile, "%s",Subdir->Name);
    fprintf(HtmlFile, "</a>    ");

    if (strcmp(Oldest,Newest)){
        char * d1, * d2;
        MakeDateString(Newest, LinkTo->Newest);

        d1 = Oldest+strlen(Oldest)-4;
        d2 = Newest+strlen(Newest)-4;
        if (memcmp(d1,d2,4) == 0){
            // If both end in same year, cut off second year.
            *d2 = '\0';
        }

        fprintf(HtmlFile,"%s - %s",Oldest, Newest);
    }else{
        fprintf(HtmlFile,Oldest);
    }

    if (!LinkTo->IsThemeDir){
        if (LinkTo->NumSubImages || LinkTo->NumSubVideos){
            fprintf(HtmlFile, "    (");
            if (LinkTo->NumSubImages){
                fprintf(HtmlFile, "%d Pictures",LinkTo->NumSubImages);
                if (LinkTo->NumSubVideos){
                    fprintf(HtmlFile," and ");
                }else{
                    fprintf(HtmlFile, ")");
                }
            }
            if (LinkTo->NumSubVideos){
                fprintf(HtmlFile, "%d Videos)",LinkTo->NumSubVideos);
            }
        }
    }
    
    fprintf(HtmlFile, "\n<p>\n",LinkTo->NumSubImages);
}

//----------------------------------------------------------------------------------
// Compare dates of two entries - helper function for qsort call.
//----------------------------------------------------------------------------------
static int SortCompareDates(const void * el1,  const void * el2)
{
    int diff;
    diff = CompareDates(&((DirEntry *)el1)->Date, &((DirEntry *)el2)->Date);

    if (diff) return diff;

    return strcmp((char *)&((DirEntry *)el1)->Name, (char *)&((DirEntry *)el2)->Name);
}


//----------------------------------------------------------------------------------
// Create browsable HTML index files for the directories.
//----------------------------------------------------------------------------------
void MakeHtmlOutput(Dir_t * Dir)
{
    VarList Images;
    VarList Directories;

    FILE * HtmlFile;
    char HtmlFileName[500];
    unsigned int a,b;
    unsigned int picno;
    char DateString[30];

    Images = Dir->Images;
    Directories = Dir->Dirs;    

    CombinePaths(HtmlFileName, Dir->FullPath, Dir->IndexName);

    HtmlFile = fopen(HtmlFileName, "w");
    if (HtmlFile == NULL){
        printf("Could not open '%s' for write\n",HtmlFileName);
        exit(-1);
    }

    // Sort FileList.
    qsort( Images.Entries, Images.NumEntries, sizeof(DirEntry), SortCompareDates);

    // Sort the directories.
    for (a=0;a<Directories.NumEntries;a++){
        // First set the date on the directories.
        Dir_t * temp;
        temp = Directories.Entries[a].Subdir;
        Directories.Entries[a].Date = temp->Oldest;
    }
    qsort( Directories.Entries, Directories.NumEntries, sizeof(DirEntry), SortCompareDates);

    // Header of file.
    fprintf(HtmlFile, "<html>\n");
    fprintf(HtmlFile, "<font size=3 face=\"helvetica\">\n"); // Switch to a nice font.


    if (!Dir->IsThemeDir){
        // Link to parent directory.
        if (!Dir->IsRootDir){
            fprintf(HtmlFile, "<a href=\"../index.html\">");
            fprintf(HtmlFile, "Parent directory");
            fprintf(HtmlFile, "</a><p>\n");
        }
    }else{
        fprintf(HtmlFile, "<a href=\"index.html\">");
        fprintf(HtmlFile, "Root directory");
        fprintf(HtmlFile, "</a><p>\n");
    }

    // If directory contains a lot of images, then list all the directories at the top.


    // Add directory links to HTML file
    for (a=0;a<Directories.NumEntries;a++){
        MakeDirEntry(HtmlFile, Dir, &Directories.Entries[a]);
    }
    fprintf(HtmlFile, "<hr>\n");

    //------------------------------------------------------------------------------
    // Videos, with thumbnails, if there is any.
    if (Dir->Videos.NumEntries){
        VarList Videos;
        Videos = Dir->Videos;
        qsort( Videos.Entries, Videos.NumEntries, sizeof(DirEntry), SortCompareDates);

        fprintf(HtmlFile,"Video clips:<p>\n");
        for (a=0;a<Videos.NumEntries;a++){
            char BaseName[260];
            strcpy(BaseName, Videos.Entries[a].Name);
            BaseName[strlen(BaseName)-4] = 0;

            fprintf(HtmlFile, "<a href=\"%s\">", Videos.Entries[a].Name);
            if (Videos.Entries[a].HasThumbnail){
                fprintf(HtmlFile, "<img width=%d height=%d align=center ", 
                    Videos.Entries[a].Width, Videos.Entries[a].Height);
                fprintf(HtmlFile, "src=\"%s.thm\"> ", BaseName);
            }
            fprintf(HtmlFile, " %s</a><p>\n", BaseName);
        }
        fprintf(HtmlFile, "<hr>\n");
    }

    //------------------------------------------------------------------------------
    

    DateString[0] = 0;
    // Add image links to HTML file
    for (picno=0;picno<Images.NumEntries;picno++){
        char TimeString[30];
        char NameString[200];

        MakeDateString(TimeString, Images.Entries[picno].Date);

        if (strcmp(DateString, TimeString)){
            // If the date has changed, put the new date in the HTML file.
            strcpy(DateString, TimeString);
            fprintf(HtmlFile, "<p>%s<br>\n",TimeString);
        }

        if (Images.Entries[picno].Size < 0){
            // this is a pseudo tag - it does not represent a picture.
            fprintf(HtmlFile,"<p>%s<br>\n",Images.Entries[picno].Name);
        }else{
            // Create an entry per picture, include filename and time for 'alt' label.
            if (Images.Entries[picno].Date.tm_hour >= 0){

                sprintf(TimeString, " (%2d:%02d%c)" ,(Images.Entries[picno].Date.tm_hour-1)%12+1,
                                                      Images.Entries[picno].Date.tm_min,
                                                      Images.Entries[picno].Date.tm_hour > 12 ? 'p' : 'a');
                
            }else{
                TimeString[0] = '\0';
            }

            fprintf(HtmlFile, "<a href=\"%s\" new >\n", EscapePath(Images.Entries[picno].Name));
            if (Images.Entries[picno].HasThumbnail){
                // Has a thumbnail image.
                fprintf(HtmlFile, "<img src=\"%s/%s\"" , smallname, EscapePath(Images.Entries[picno].Name));
            }else{
                fprintf(HtmlFile, "<img src=\"%s\"" , EscapePath(Images.Entries[picno].Name));
            }

            strcpy(NameString, Images.Entries[picno].Name);
            for (b=strlen(NameString);b>=0;){
                b--;
                if (NameString[b] == '.'){
                    NameString[b] = 0;
                    break;
                }
            }

            fprintf(HtmlFile, " alt=\"%s%s\" ", NameString, TimeString);
            fprintf(HtmlFile, " title=\"%s%s\" ", NameString, TimeString);

            {
                // Work out how wide the thumbnail is based on having told mogrify to make it 240 tall.
                int w;
                w = (Images.Entries[picno].Width * 240 + 120) / Images.Entries[picno].Height;
                fprintf(HtmlFile, "width=%d height=240", w);
            }

            /*
            #ifdef STORE_FILEDATE
                if (Images.Entries[picno].FileDate < 1040998793){
                    // Old image
                    fprintf(HtmlFile, " border=0></A>\n");
                }else{
                    // New image
                    fprintf(HtmlFile, " border=2></A>\n");
                }
            #else*/
            fprintf(HtmlFile, " border=0></A>\n");
        }

        if (Images.NumEntries >= 4){
            // If any of the subdirectories fit between two images, 
            // put a subdirectory tag in here as well - but only if there's 4 or more images.
            // if dir_start_time >= prev_pic_time && dir_end_time < next_pic_time

            for (a=0;a<Directories.NumEntries;a++){
                Dir_t * temp;
                temp = Directories.Entries[a].Subdir;

                if (temp->Newest.tm_year <= 0) continue; // Date unknown for this dir.

                if (CompareDates(&Images.Entries[picno].Date,&temp->Oldest) <= 0){
                    if (picno >= Images.NumEntries-1 || 
                        CompareDates(&Images.Entries[picno+1].Date,&temp->Newest) > 0){
                        // This directory fits in between the two pictures.
                        fprintf(HtmlFile, "<p>");
                        MakeDirEntry(HtmlFile, Dir, &Directories.Entries[a]);
                    }
                }
            }
        }
    }
    fclose(HtmlFile);
}

//----------------------------------------------------------------------------------
// Go through the built up data in memory and create directories.
//----------------------------------------------------------------------------------
void CreateIndexFiles(Dir_t * Dir)
{
    int n;
    int a;
    DirEntry * Entries;

    if (Dir->IsThemeDir){
        printf("(%s)\n", Dir->Theme);
    }

//    printf("Dir: %s\n",Dir->FullPath);

    if (Dir->NoAutoGenerate){
        // This is a manually created index.  Just leave it alone.
        return;
    }

    MakeHtmlOutput(Dir);

    n = Dir->Dirs.NumEntries;
    Entries = Dir->Dirs.Entries;

    for (a=0;a<n;a++){
        Dir_t * Subdir;
        Subdir = Entries[a].Subdir;
        if (!Entries[a].DoNotDescend){
            CreateIndexFiles(Subdir);
        }
    }
}

