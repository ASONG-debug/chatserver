set -x
rm -rf `pwd`/build/*
cd `pwd`/build &&
cmake .. &&
make -j

