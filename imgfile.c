//--------------------------------------------------------------------------
// imgfile.c - Thunking layer for jhead to handle both JPEG and PNG
//--------------------------------------------------------------------------

#include "jhead.h"
#include <sys/stat.h>

// Define image types
#define TYPE_UNKNOWN 0
#define TYPE_JPEG    1
#define TYPE_PNG     2
int ImgTypeLoaded = TYPE_UNKNOWN;

// Storage for simplified info extracted from file.
ImageInfo_t ImageInfo;

void DiscardImgData(void)
{
    if (ImgTypeLoaded == TYPE_JPEG) DiscardJpegData();
    if (ImgTypeLoaded == TYPE_PNG) DiscardPngData();
}

//--------------------------------------------------------------------------
// Process contents of COM marker or PNG 'tEXt' section
// We want to print out the marker contents as legible text;
// we must guard against random junk and varying newline representations.
// Not taking utf8 into consideration.
//--------------------------------------------------------------------------
void ProcessImgComment (const uchar * Data, int length)
{
    int ch;
    char Comment[MAX_COMMENT_SIZE+1];
    int nch;
    int a;

    nch = 0;

    if (length > MAX_COMMENT_SIZE) length = MAX_COMMENT_SIZE; // Truncate if it won't fit in our structure.

    for (a=0;a<length;a++){
        ch = Data[a];

        if (ch == '\r' && a < length-1 && Data[a+1] == '\n') continue; // Remove cr followed by lf.

        if (ch >= 32 || ch == '\n' || ch == '\t'){
            Comment[nch++] = (char)ch;
        }else{
            Comment[nch++] = '?';
        }
    }

    Comment[nch] = '\0'; // Null terminate

    if (ShowTags){
        if (ImgTypeLoaded == TYPE_JPEG) printf("COM marker comment: %s\n",Comment);
        if (ImgTypeLoaded == TYPE_PNG) printf("tEXt section comment: %s\n",Comment);
    }

    strcpy(ImageInfo.Comments,Comment);
}

//--------------------------------------------------------------------------
// Thunked Read Function
//--------------------------------------------------------------------------
int ReadImgFile(const char * FileName, ReadMode_t ReadMode) {
    unsigned char Sig[8];
    FILE * f = fopen(FileName, "rb");

    if (!f) {
        fprintf(stderr, "can't open '%s'\n", FileName);
        return FALSE;
    }

    int read = fread(Sig, 1, 8, f);
    fseek(f,0,SEEK_SET);

    //printf("Sig: %02x %02x\n",Sig[0],Sig[1]);

    if (read >= 2 && Sig[0] == 0xff && Sig[1] == 0xd8){
        ImgTypeLoaded = TYPE_JPEG;
    }

    if (read >= 8 && memcmp(Sig, "\x89PNG\r\n\x1a\n", 8) == 0){
        ImgTypeLoaded = TYPE_PNG;
    }


    if (ImgTypeLoaded == TYPE_JPEG) {
        return ReadJpegFile(f, ReadMode);
    } else if (ImgTypeLoaded == TYPE_PNG) {
        return ReadPngFile(f, ReadMode);
    }

    fclose(f);
    fprintf(stderr, "Not JPEG or PNG: %s\n", FileName);

    return FALSE;
}

//--------------------------------------------------------------------------
// Thunked Write Function
//--------------------------------------------------------------------------
void WriteImgFile(const char * FileName)
{
    if (ImgTypeLoaded == TYPE_JPEG) WriteJpegFile(FileName);
    if (ImgTypeLoaded == TYPE_PNG) WritePngFile(FileName);
}

void DiscardAllButExif(){
    if (ImgTypeLoaded == TYPE_JPEG) DiscardAllJpegButExif();
    if (ImgTypeLoaded == TYPE_PNG) DiscardAllPngButExif();
}

int ReplaceImgThumbnail(const char * ThumbFileName)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return ReplaceJpegThumbnail(ThumbFileName);
    }else{
        ErrFatal("not implemented 1\n");
        return FALSE;
    }
}

//--------------------------------------------------------------------------
// Remove a certain type of section.
//--------------------------------------------------------------------------
int RemoveImgSectionByType(int SectionType)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return RemoveJpegSectionByType(SectionType);
    }else{
        ErrFatal("Not implemented 1.5");
        return 0;
    }
}


int RemoveUnknownImgSections(void)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return RemoveUnknownJpegSections();
    }else{
        ErrFatal("not implemented 2\n");
        return FALSE;
    }
}

uchar * GetImgExifSectionData(unsigned int *Size)
{
    ImgSect_t * ExSection;
    if (ImgTypeLoaded == TYPE_JPEG){
        ExSection = FindJpegSection(M_EXIF);
        if (ExSection == NULL) return NULL;
        if (Size) *Size = ExSection->Size-8;
        return ExSection->Data+8;
    }else if (ImgTypeLoaded == TYPE_PNG){
        ExSection = FindPngSection(0x65584966); // 'eXIf'
        if (ExSection == NULL) return NULL;
        if (Size) *Size = ExSection->Size;
        return ExSection->Data;
    }
    return NULL;
}

//--------------------------------------------------------------------------
// Remove Exif section from the image
//--------------------------------------------------------------------------
int RemoveImgExif(void)
{
    if (ImgTypeLoaded == TYPE_JPEG) {
        return RemoveJpegSectionByType(M_EXIF);
    } else if (ImgTypeLoaded == TYPE_PNG) {
        return RemovePngSectionByType(0x65584966); // 'eXIf'
    }
    return FALSE;
}

//--------------------------------------------------------------------------
// Create a minimal Exif header
//--------------------------------------------------------------------------
void CreateImgExif(void)
{
    if (ImgTypeLoaded == TYPE_JPEG) {
        CreateMinimalJpegExif();
    } else if (ImgTypeLoaded == TYPE_PNG) {
        CreateMinimalPngExif();
    }
}

//--------------------------------------------------------------------------
// Trim redundant zeros off the end of an exif header
//--------------------------------------------------------------------------
int TrimImgExifTrailingZeros()
{
    unsigned Size;
    uchar * ExifData;
    ExifData = GetImgExifSectionData(&Size);
    if (!ExifData) return FALSE;

    unsigned NewSize = ExifBytesActuallyUsed(ExifData, Size);
    if (NewSize < Size){
        ImgSect_t * ExSec;
        if (ImgTypeLoaded == TYPE_JPEG){
            ExSec = FindJpegSection(M_EXIF);
            ExSec->Size = NewSize+8;
            return TRUE;
        } else if (ImgTypeLoaded == TYPE_PNG) {
            ExSec = FindPngSection(0x65584966); // 'eXIf'
            ExSec->Size = NewSize;
            return TRUE;
        }
    }
    return FALSE;
}


void SetImgCommentTo(char * NewComment)
{
    if (ImgTypeLoaded == TYPE_JPEG) {
        SetJpegCommentTo(NewComment);
    } else if (ImgTypeLoaded == TYPE_PNG) {
        SetPngCommentTo(NewComment);
    } else {
        ErrFatal("No image loaded to set comment");
    }
}

ImgSect_t * FindImgSection(int SectionType)
{
    // Section type values differ between Jpeg and Png, so calls to this
    // function needs to be replaced with calls referring to specific section types.
    if (ImgTypeLoaded == TYPE_JPEG){
        return FindJpegSection(SectionType);
    }else{
        if (SectionType == M_IPTC) return NULL; // Not used in PNG files

        printf("want %d\n",SectionType);
        ErrFatal("not implemented 4\n");
        return NULL;
    }
}

ImgSect_t * CreateImgSection(int SectionType, unsigned char * Data, int Size)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return CreateJpegSection(SectionType, Data, Size);
    }else{
        ErrFatal("not implemented 5\n");
        return NULL;
    }
}

int SaveImgThumbnail(char * ThumbFileName)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return SaveJpegThumbnail(ThumbFileName);
    }else{
        ErrFatal("not implemented 6\n");
        return FALSE;
    }
}

//--------------------------------------------------------------------------
// Handle renaming of files by date.
//--------------------------------------------------------------------------
static int RenameSequence = 0;
void DoFileRenaming(const char * FileName, char * strftime_args)
{
    int PrefixPart = 0; // Where the actual filename starts.
    int ExtensionPart;  // Where the file extension starts.
    int a;
    struct tm tm;
    char NewBaseName[PATH_MAX*2];
    int AddLetter = 0;
    char NewName[PATH_MAX+2];

    ExtensionPart = strlen(FileName);
    for (a=0;FileName[a];a++){
        if (FileName[a] == SLASH){
            // Don't count path component.
            PrefixPart = a+1;
        }

        if (FileName[a] == '.') ExtensionPart = a;  // Remember where extension starts.
    }
    if (ExtensionPart < PrefixPart) { // no extension found
        ExtensionPart = strlen(FileName);
    }

    if (!Exif2tm(&tm, ImageInfo.DateTime)){
        printf("File '%s' contains no exif date stamp.  Using file date\n",FileName);
        // Use file date/time instead.
        tm = *localtime(&ImageInfo.FileDateTime);
    }


    strncpy(NewBaseName, FileName, PATH_MAX); // Get path component of name.

    if (strftime_args){
        // Complicated scheme for flexibility.  Just pass the args to strftime.
        time_t UnixTime;

        char *s;
        char pattern[PATH_MAX+20];
        int n = ExtensionPart - PrefixPart;

        // Call mktime to get weekday and such filled in.
        UnixTime = mktime(&tm);
        if ((int)UnixTime == -1){
            printf("Could not convert %s to unix time",ImageInfo.DateTime);
            return;
        }

        // Substitute "%f" for the original name (minus path & extension)
        pattern[PATH_MAX-1]=0;
        strncpy(pattern, strftime_args, PATH_MAX-1);
        while ((s = strstr(pattern, "%f")) && strlen(pattern) + n < PATH_MAX-1){
            memmove(s + n, s + 2, strlen(s+2) + 1);
            memmove(s, FileName + PrefixPart, n);
        }

        {
            // Sequential number renaming part.
            // '%i' type pattern becomes sequence number.
            int ppos = -1;
            for (a=0;pattern[a];a++){
                if (pattern[a] == '%'){
                     ppos = a;
                }else if (pattern[a] == 'i'){
                    if (ppos >= 0 && a<ppos+4){
                        // Replace this part with a number.
                        char pat[8], num[16];
                        int l,nl;
                        memcpy(pat, pattern+ppos, 4);
                        pat[a-ppos] = 'd'; // Replace 'i' with 'd' for '%d'
                        pat[a-ppos+1] = '\0';
                        sprintf(num, pat, ++RenameSequence); // let printf do the number formatting.
                        nl = strlen(num);
                        l = strlen(pattern+a+1);
                        if (ppos+nl+l+1 >= PATH_MAX) ErrFatal("str overflow");
                        memmove(pattern+ppos+nl, pattern+a+1, l+1);
                        memcpy(pattern+ppos, num, nl);
                        break;
                    }
                }else if (!isdigit(pattern[a])){
                    ppos = -1;
                }
            }
        }
        strftime(NewName, PATH_MAX, pattern, &tm);
    }else{
        // My favourite scheme.
        sprintf(NewName, "%02d%02d-%02d%02d%02d",
             tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    NewBaseName[PrefixPart] = 0;
    CatPath(NewBaseName, NewName);

    AddLetter = isdigit(NewBaseName[strlen(NewBaseName)-1]);
    for (a=0;;a++){
        char NewName[PATH_MAX*2+10];
        char NameExtra[3];
        struct stat dummy;

        if (a){
            // Generate a suffix for the file name if previous choice of names is taken.
            // depending on whether the name ends in a letter or digit, pick the opposite to separate
            // it.  This to avoid using a separator character - this because any good separator
            // is before the '.' in ascii, and so sorting the names would put the later name before
            // the name without suffix, causing the pictures to more likely be out of order.
            if (AddLetter){
                NameExtra[0] = (char)('a'-1+a); // Try a,b,c,d... for suffix if it ends in a number.
            }else{
                NameExtra[0] = (char)('0'-1+a); // Try 0,1,2,3... for suffix if it ends in a latter.
            }
            NameExtra[1] = 0;
        }else{
            NameExtra[0] = 0;
        }

        char * FileNameExt = "jpg";
        if (ImgTypeLoaded == TYPE_PNG) FileNameExt = "png";

        snprintf(NewName, sizeof(NewName), "%s%s.%s", NewBaseName, NameExtra, FileNameExt);

        if (!strcmp(FileName, NewName)) break; // Skip if its already this name.

        if (!EnsurePathExists(NewBaseName)){
            break;
        }


        if (stat(NewName, &dummy)){
            // This name does not pre-exist.
            if (rename(FileName, NewName) == 0){
                printf("%s --> %s\n",FileName, NewName);
            }else{
                printf("Error: Couldn't rename '%s' to '%s'\n",FileName, NewName);
            }
            break;

        }

        if (a > 25 || (!AddLetter && a > 9)){
            printf("Possible new names for for '%s' already exist\n",FileName);
            break;
        }
    }
}

//--------------------------------------------------------------------------
// Initialisation.
//--------------------------------------------------------------------------
void ResetImgfile(void)
{
    ResetJpegFile();
    ResetPngFile();
}
