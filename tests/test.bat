copy ..\jhead.exe

rm -f results-txt\*
rm -f results-bin\*

rem The date of the files has to be set deterministically.
copy normal-digicams\*.jpg results-bin
jhead -ft results-bin\*.jpg

rem test parsing of normal exif information.
cd results-bin
FOR %%i IN (*.jpg) DO jhead -v %%i > ..\results-txt\%%i
cd ..

rem test parsing and display of some strange jpeg files.

rem FIXME - this is broken - add some kind of supress file time option...
cd results-bin
FOR %%i IN (*.jpg) DO jhead -v %%i > ..\results-txt\%%i
cd ..

rem -------------------------------------------------------------------
rem test comment manipulation
copy normal-digicams\S100.jpg comments.jpg
rem insert a comment (-ci)
jhead -ci normal-digicams\testfile.txt -ft comments.jpg
rem extract the comments (-cs)
jhead -cs results-txt\comments.txt comments.jpg 
jhead comments.jpg >> results-txt\comments.txt
rem test comment edit
set EDITOR=normal-digicams\pretend-editor.bat
jhead -ce -ft comments.jpg
jhead comments.jpg >> results-txt\comments.txt

copy comments.jpg results-bin
rem replace comment with a literal comment
jhead -cl "a literla inserted comment!" -ft comments.jpg
jhead comments.jpg >> results-txt\comments.txt

rem remove the comment again (-dc)
jhead -dc -ft comments.jpg
jhead comments.jpg >> results-txt\comments.txt


rem -------------------------------------------------------------------
rem test rotate command stuff
copy normal-digicams\rotate.jpg results-bin
jhead -norot -ft results-bin\rotate.jpg
jhead results-bin\rotate.jpg > results-txt\rotate.txt
copy normal-digicams\rotate.jpg results-bin
jhead -autorot -ft results-bin\rotate.jpg
jhead results-bin\rotate.jpg >> results-txt\rotate.txt

