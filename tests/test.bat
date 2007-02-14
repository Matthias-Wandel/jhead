@setlocal
copy ..\jhead.exe

rm -f results-txt\*
rm -f results-bin\*

rem test parsing of normal exif information.
FOR %%I IN (normal-digicams\*.jpg) DO jhead -v -nofinfo %%I > results-txt\%%~nI.jpg

rem test parsing and display of some strange jpeg files.
FOR %%I IN (strange-jpegs\*.jpg) DO jhead -v -nofinfo %%I > results-txt\%%~nI.jpg

rem -------------------------------------------------------------------
rem test compact view
echo Normal digicams > results-txt\compact-txt
jhead -c normal-digicams\*.jpg >> results-txt\compact-txt
echo Strange jpegs >> results-txt\compact-txt
jhead -c strange-jpegs\*.jpg >> results-txt\compact-txt
echo Portrait only option test >> results-txt\compact-txt
jhead -c -se -orp normal-digicams\*.jpg >> results-txt\compact-txt

rem -------------------------------------------------------------------
rem test date minaipulation
copy normal-digicams\3dmsc.jpg time.jpg
jhead -ta+2:00 time.jpg
jhead -ft time.jpg
jhead time.jpg > results-txt\time.txt
jhead -ta-0:00:20 time.jpg
jhead -nofinfo time.jpg >> results-txt\time.txt
jhead -da+2005:05:01/01:01-2004:05:01 time.jpg
jhead -nofinfo time.jpg >> results-txt\time.txt
copy strange-jpegs\badyear.jpg badyear.jpg
jhead -nofinfo badyear.jpg >> results-txt\time.txt
jhead -ds2006:01 -ta+1:00 badyear.jpg >> results-txt\time.txt
jhead -nofinfo badyear.jpg >> results-txt\time.txt

rem -------------------------------------------------------------------
rem test comment manipulation
copy normal-digicams\S100.jpg comments.jpg
rem insert a comment (-ci)
jhead -ci normal-digicams\testfile.txt -ft comments.jpg
rem extract the comments (-cs)
jhead -cs results-txt\comments.txt comments.jpg 
jhead -nofinfo comments.jpg >> results-txt\comments.txt
rem test comment edit
set EDITOR=normal-digicams\pretend-editor.bat
jhead -ce -ft comments.jpg
jhead -nofinfo comments.jpg >> results-txt\comments.txt

copy comments.jpg results-bin
rem replace comment with a literal comment
jhead -cl "a literal inserted comment!" -ft comments.jpg
jhead -nofinfo comments.jpg >> results-txt\comments.txt

rem remove the comment again (-dc)
jhead -dc -ft comments.jpg
jhead -nofinfo comments.jpg >> results-txt\comments.txt


rem -------------------------------------------------------------------
rem test rotate command stuff
copy normal-digicams\rotate.jpg results-bin
jhead -norot -ft results-bin\rotate.jpg
jhead results-bin\rotate.jpg > results-txt\rotate.txt
copy normal-digicams\rotate*.jpg results-bin
jhead -autorot -ft results-bin\rotate*.jpg
jhead -nofinfo results-bin\rotate*.jpg >> results-txt\rotate.txt

rem test with two orientation  tags.
copy normal-digicams\two-orientation.jpg results-bin
jhead -autorot results-bin\two-orientation.jpg
jhead -nofinfo -v results-bin\two-orientation.jpg >> results-txt\rotate.txt

rem test saving of incorrectly placed thumbnail
jhead -st results-bin\thumbnail-place-error.thm strange-jpegs\thumbnail-place-error.jpg

rem -------------------------------------------------------------------
rem test rename command.
rm -rf temp
mkdir temp
copy normal-digicams\*.jpg temp
del temp\no-exif.jpg
jhead -nf%%02i-%%H-%%f temp\*.jpg
dir /b temp >> results-txt\new-names-txt

rem -------------------------------------------------------------------
rem test -purejpg with an image that I used to corrupt.
copy strange-jpegs\olav.jpg results-bin
jhead -purejpg results-bin\olav.jpg
jhead -v -nofinfo results-bin\olav.jpg > results-txt\olav.jpg

rem -------------------------------------------------------------------
rem Test deletion of a thumbnail, and replacing of a thumbnail

copy normal-digicams\fuji-dx10.jpg results-bin\thumb-deleted.jpg
jhead -dt -ft results-bin\thumb-deleted.jpg
copy results-bin\thumb-deleted.jpg results-bin\thumb-inserted.jpg
rem reinsert a different thumbnail
jhead -rt normal-digicams\no-exif.jpg results-bin\thumb-inserted.jpg
rem replace thumbnail in one step
copy normal-digicams\olympus.jpg results-bin\thumb-replaced.jpg
jhead -rt normal-digicams\no-exif.jpg results-bin\thumb-replaced.jpg
jhead -v -nofinfo res*bin\thumb-*.jpg > results-txt\thumb-operations-txt

rem -------------------------------------------------------------------
rem Test generating a thumbnail
copy normal-digicams\fuji-dx10.jpg results-bin\exif-regen.jpg
jhead -mkexif results-bin\exif-regen.jpg
rem Test regenerating using old exif date
jhead -v -nofinfo results-bin\exif-regen.jpg > results-txt\exif-regen-txt
jhead -ft -de restuls-bin\exif-regen.jpg
rem Test regenerating using file time for timestamp.  also create thumnail.
jhead -mkexif -rgt results-bin\exif-regen.jpg
jhead -v -nofinfo results-bin\exif-regen.jpg >> results-txt\exif-regen-txt

rem -------------------------------------------------------------------
rem Test IPTC manipulation
copy strange-jpegs\iptc?.jpg results-bin\
jhead -di results-bin\iptc1.jpg
rem test removal and restoration of iptc section
jhead -de -cmd "jhead -purejpg &i" results-bin\iptc2.jpg
jhead results-bin\iptc?.jpg > results-txt\iptc-txt
