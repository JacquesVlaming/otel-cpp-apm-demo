# otel-cpp-demo

Based on https://opentelemetry.io/docs/languages/cpp/getting-started/

Prerequisites
Ensure that you have the following installed locally:

Git
C++ compiler supporting C++ version >= 14
Make
CMake version >= 3.25
OpenSSL
Zlib

Steps
sudo apt update
sudo apt install git
sudo apt install build-essential
sudo apt install cmake
sudo apt install libssl-dev
sudo apt install zlib1g-dev

git clone https://github.com/elastic/otel-cpp-demo.git

cd otel-cpp-demo

git clone https://github.com/oatpp/oatpp.git

cd oatpp

git checkout 1.3.0-latest

mkdir build
cd build

cmake ..
make

sudo make install

cd $HOME/otel-cpp-demo/
git clone https://github.com/open-telemetry/opentelemetry-cpp.git
cd opentelemetry-cpp
mkdir build
cd build

cmake -DBUILD_SHARED_LIBS=ON -DWITH_EXAMPLES=OFF -DWITH_OTLP_GRPC=ON -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF ..

cmake --build . -j$(nproc)

cmake --install . --prefix ../../otel-cpp








