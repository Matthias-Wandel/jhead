# run this script with "source turnon_asan.sh"
# Enable ASan options and bypass link order verification
export ASAN=1
export ASAN_OPTIONS=verify_asan_link_order=0

echo now "make clean" then "make" to compile it into jhead.