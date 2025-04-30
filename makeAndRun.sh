set -e
#mkdir build
cd build
#cmake ..
make -j16
cd -
./build/PPSSPPSDL
