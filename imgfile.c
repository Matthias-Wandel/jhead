//--------------------------------------------------------------------------
// imgfile.c - Thunking layer for jhead to handle both JPEG and PNG
//--------------------------------------------------------------------------

#include "jhead.h"
#include <sys/stat.h>

// Storage for simplified info extracted from file.
ImageInfo_t ImageInfo;

void DiscardImgData(void)
{
    switch (ImageInfo.ImgTypeLoaded){
        case IMG_TYPE_JPEG: DiscardJpegData();break;
        case IMG_TYPE_PNG: DiscardPngData();break;
        case IMG_TYPE_WEBP: DiscardWebpData();break;
    }
}

//--------------------------------------------------------------------------
// Process contents of COM marker or PNG 'tEXt' section
// We want to print out the marker contents as legible text;
// we must guard against random junk and varying newline representations.
// Not taking utf8 into consideration.
//--------------------------------------------------------------------------
void ProcessImgComment (const uchar * Data, int length)
{
    memset(ImageInfo.Comments, 0, sizeof(ImageInfo.Comments));

    if (length > sizeof(ImageInfo.Comments)-1) length = sizeof(ImageInfo.Comments)-1;
    strncpy(ImageInfo.Comments,Data, length);

    if (ShowTags){
        char * type = "??";
        switch (ImageInfo.ImgTypeLoaded){
            case IMG_TYPE_JPEG: type = "COM"; break;
            case IMG_TYPE_PNG: type = "tEXt"; break;
            case IMG_TYPE_WEBP: type = "COMM"; break;
        }

        printf("%s section comment: %s\n", type, ImageInfo.Comments);
    }
}

//--------------------------------------------------------------------------
// Thunked Read Function
//--------------------------------------------------------------------------
int ReadImgFile(const char * FileName, ReadMode_t ReadMode) {
    unsigned char Sig[12];
    FILE * f = fopen(FileName, "rb");
    int retval;

    if (!f) {
        fprintf(stderr, "can't open '%s'\n", FileName);
        return FALSE;
    }

    int read = fread(Sig, 1, 12, f);
    if (read < 12){
        // Too small to contain an image.
        fclose(f);
        return FALSE;
    }
    fseek(f,0,SEEK_SET);

    //printf("Sig: %02x %02x\n",Sig[0],Sig[1]);

    if (Sig[0] == 0xff && Sig[1] == 0xd8) {
        ImageInfo.ImgTypeLoaded = IMG_TYPE_JPEG;
        retval = ReadJpegFile(f, ReadMode);
    }else if (memcmp(Sig, "\x89PNG\r\n\x1a\n", 8) == 0) {
        ImageInfo.ImgTypeLoaded = IMG_TYPE_PNG;
        retval = ReadPngFile(f, ReadMode);
    }else if (memcmp(Sig, "RIFF", 4) == 0 && memcmp(Sig+8, "WEBP", 4) == 0) {
        ImageInfo.ImgTypeLoaded = IMG_TYPE_WEBP;
        retval = ReadWebpSections(f, ReadMode);
    }else{
        printf("Unhandled file type (not JPG, PNG or WEBP): %s\n",FileName);
    }
    fclose(f);
    return retval;
}

//--------------------------------------------------------------------------
// Thunked Write Function
//--------------------------------------------------------------------------
void WriteImgFile(const char * FileName) {
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG) WriteJpegFile(FileName);
    else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) WritePngFile(FileName);
    else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) WriteWebpFile(FileName);
}

int RemoveMetadataImgSections(void)
{
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG){
        return RemoveMetadataJpegSections();
    }else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG){
        return RemoveMetadataPngSections();
    }else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP){
        return RemoveMetadataWebpSections();
    }
}

//--------------------------------------------------------------------------
// Get pointer to exif section, cause we want to change it.
//--------------------------------------------------------------------------
uchar * GetImgExifSectionData(unsigned int *Size)
{
    ImgSect_t * ExSection;
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG){
        ExSection = FindJpegSection(M_EXIF);
        if (ExSection == NULL) return NULL;
        if (Size) *Size = ExSection->Size-8;
        return ExSection->Data+8;
    }else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG){
        ExSection = FindPngSection(0x65584966); // 'eXIf'
        if (ExSection == NULL) return NULL;
        if (Size) *Size = ExSection->Size;
        return ExSection->Data;
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) {
        ExSection = GetWebpSection(0x45584946);  //'EXIF'
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
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG) {
        return RemoveJpegSectionByType(M_EXIF);
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) {
        return RemovePngSectionByType(0x65584966); // 'eXIf'
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) {
        return RemoveWebpSectionByType(0x45584946); // "EXIF" section
    }else{
        ErrFatal("Not implemented for image type\n");
    }
    return FALSE;
}
int RemoveImgXmp(void)
{
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG) {
        return RemoveJpegSectionByType(M_XMP);
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) {
        return RemoveWebpSectionByType(0x584d5020); // "XMP " section
    }else{
        ErrFatal("Remove XMP not implemented for PNG\n");
    }
    return FALSE;
}

//--------------------------------------------------------------------------
// Create a minimal Exif header
//--------------------------------------------------------------------------
void CreateImgExif(void)
{
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG) {
        CreateMinimalJpegExif();
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) {
        CreateMinimalPngExif();
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) {
        CreateMinimalWebpExif();
    }else{
        ErrFatal("Not implemented for image type\n");
    }


}

uchar * ChangeExifSectionLength(int NewLength)
{
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG) {
        return ChangeJpegExifSectionLength(NewLength);
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) {
        return ChangePngExifSectionLength(NewLength);
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) {
        return ChangeWebpExifSectionLength(NewLength);
    }else{
        ErrFatal("Not implemented for image type");
        return 0;
    }
}


//--------------------------------------------------------------------------
// Replace or remove exif thumbnail
//--------------------------------------------------------------------------
int ReplaceImgThumbnail(const char * ThumbFileName)
{
    FILE * ThumbnailFile;
    int ThumbLen;
    uchar * ThumbnailPointer;

    if (ImageInfo.ThumbnailOffset == 0 || ImageInfo.ThumbnailAtEnd == FALSE){
        if (ThumbFileName == NULL){
            // Delete of nonexistent thumbnail (not even pointers present)
            // No action, no error.
            return FALSE;
        }

        // Adding or removing of thumbnail is not possible - that would require rearranging
        // of the exif header, which is risky, and jhead doesn't know how to do.
        printf("Image contains no thumbnail to replace - add is not possible\n");
        return FALSE;
    }

    if (ThumbFileName){
        ThumbnailFile = fopen(ThumbFileName,"rb");

        if (ThumbnailFile == NULL){
            noread:
            ErrFatal("Could not read thumbnail file");
            return FALSE;
        }

        // get length
        fseek(ThumbnailFile, 0, SEEK_END);

        ThumbLen = ftell(ThumbnailFile);
        fseek(ThumbnailFile, 0, SEEK_SET);

        if (ThumbLen + ImageInfo.ThumbnailOffset > 0x10000-20){
            ErrFatal("Thumbnail is too large to insert into exif header");
        }
    }else{
        if (ImageInfo.ThumbnailSize == 0){
             return FALSE;
        }
        ThumbLen = 0;
        ThumbnailFile = NULL;
    }

    unsigned char * ExifSection;
    ExifSection = ChangeExifSectionLength(ImageInfo.ThumbnailOffset+ThumbLen);

    ThumbnailPointer = ExifSection+ImageInfo.ThumbnailOffset;

    if (ThumbnailFile){
        int nread = fread(ThumbnailPointer, 1, ThumbLen, ThumbnailFile);
        if (nread != ThumbLen){
            goto noread;
        }
        fclose(ThumbnailFile);
    }

    ImageInfo.ThumbnailSize = ThumbLen;

    // Write the newe length into the exif file system.
    Put32u(ExifSection+ImageInfo.ThumbnailSizeOffset, ThumbLen);

    return TRUE;

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
        if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG){
            ExSec = FindJpegSection(M_EXIF);
            ExSec->Size = NewSize+8;
            return TRUE;
        } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) {
            ExSec = FindPngSection(0x65584966); // 'eXIf'
            ExSec->Size = NewSize;
            return TRUE;
        }else{
            ErrFatal("trim not implemented for file type");
        }
    }
    return FALSE;
}


void SetImgCommentTo(char * NewComment)
{
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG) {
        SetJpegCommentTo(NewComment);
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) {
        SetPngCommentTo(NewComment);
    } else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) {
        SetWebpCommentTo(NewComment);
    } else {
        ErrFatal("No image loaded to set comment");
    }
}

ImgSect_t * FindImgSection(int SectionType)
{
    // Section type values differ between Jpeg and Png, so calls to this
    // function needs to be replaced with calls referring to specific section types.
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG){
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
    if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG){
        return CreateJpegSection(SectionType, Data, Size);
    }else{
        ErrFatal("not implemented 5\n");
        return NULL;
    }
}

//--------------------------------------------------------------------------
// Replace or remove exif thumbnail
//--------------------------------------------------------------------------
int SaveImgThumbnail(char * ThumbFileName)
{
    FILE * ThumbnailFile;

    if (ImageInfo.ThumbnailOffset == 0 || ImageInfo.ThumbnailSize == 0){
        printf("Image contains no thumbnail\n");
        return FALSE;
    }

    if (strcmp(ThumbFileName, "-") == 0){
        // A filename of '-' indicates thumbnail goes to stdout.
        // This doesn't make much sense under Windows, so this feature is unix only.
        ThumbnailFile = stdout;
    }else{
        ThumbnailFile = fopen(ThumbFileName,"wb");
    }

    if (ThumbnailFile){
        uchar * ThumbnailPointer;
        ImgSect_t * ExifSection;

        if (ImageInfo.ImgTypeLoaded == IMG_TYPE_JPEG){
            ExifSection = FindJpegSection(M_EXIF);
            ThumbnailPointer = ExifSection->Data+ImageInfo.ThumbnailOffset+8;
        }else{
            if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP){
                ExifSection = GetWebpSection(0x45584946);
            }else if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG){
                ExifSection = FindPngSection(0x65584966); //'eXIF'
            }else{
                ErrFatal("not implemented for file type\n");
            }
            ThumbnailPointer = ExifSection->Data+ImageInfo.ThumbnailOffset;
        }

        fwrite(ThumbnailPointer, ImageInfo.ThumbnailSize ,1, ThumbnailFile);
        fclose(ThumbnailFile);
        return TRUE;
    }else{
        ErrFatal("Could not write thumbnail file");
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
        if (ImageInfo.ImgTypeLoaded == IMG_TYPE_PNG) FileNameExt = "png";
        if (ImageInfo.ImgTypeLoaded == IMG_TYPE_WEBP) FileNameExt = "webp";

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
    ResetWebpFile();
}
