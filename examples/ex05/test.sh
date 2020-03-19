rm -r build
mkdir build
cd build
cmake -DBxDecay0_DIR=$(bxdecay0-config --cmakedir) ..
make
cd ..
./build/ex05

