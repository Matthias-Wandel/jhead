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
jhead -c normal-digicams\*.jpg > results-txt\compact-txt
jhead -c strange-jpegs\*.jpg >> results-txt\compact-txt

rem -------------------------------------------------------------------
rem test odd date format
copy normal-digicams\3dmsc.jpg time.jpg
jhead -ta+2:00 time.jpg
jhead -ft time.jpg
jhead time.jpg > results-txt\time.txt
jhead -ta-0:00:20 time.jpg
jhead -nofinfo time.jpg >> results-txt\time.txt

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
copy normal-digicams\rotate.jpg results-bin
jhead -autorot -ft results-bin\rotate.jpg
jhead -nofinfo results-bin\rotate.jpg >> results-txt\rotate.txt

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
jhead -nf%%02i-%%H-%%f temp\*.jpg
ls temp >> results-txt\new-names-txt
