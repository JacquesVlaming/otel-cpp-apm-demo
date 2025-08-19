Host: Redhat 9

Install docker:
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/rhel/docker-ce.repo
sudo dnf update
sudo dnf install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo systemctl enable --now docker

Install: Git, CMake, GCC and Dev Tools
sudo dnf install git -y
sudo dnf install cmake -y
sudo dnf groupinstall "Development Tools" -y
sudo dnf install gcc-c++ make -y

In my case I used /mnt/otel as a working directory
mkdir /mnt/otel
export TMPDIR=/mnt/otel
cd /mnt/otel

Clone the demo repo:
git clone https://github.com/JacquesVlaming/otel-cpp-apm-demo.git

Clone the OTEL C++ repo:
git clone https://github.com/open-telemetry/opentelemetry-cpp.git

Complie the SDK:
cd opentelemetry-cpp/
mkdir build && cd build





cd $TMPDIR


cmake .. \
-DBUILD_SHARED_LIBS=ON \
-DWITH_OTLP_GRPC=ON \
-DCMAKE_INSTALL_PREFIX=$TMPDIR/otel-cpp/install \
-DWITH_EXAMPLES=OFF \
-DBUILD_TESTING=OFF

make -j$(nproc)
sudo make install
sudo ldconfig


[//]: # (g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so -I /mnt/otel/otel-cpp/install/include -L /mnt/otel/otel-cpp/otel-cpp/install/lib -l dl -l pthread)
g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so -I $TMPDIR/otel-cpp/install/include -L $TMPDIR/otel-cpp/otel-cpp/install/lib -l dl -l pthread
g++ -std=c++17 -Wall -o ads_client ads_client.cpp
g++ -std=c++17 -Wall -pthread -o ads_server ads_server.cpp


export TMPDEMODIR=/mnt/otel/otel-cpp-apm-demo

docker run --rm -it \
-v $TMPDEMODIR/otel-collector-config.yaml:/etc/otel-collector-config.yaml \
-p 4317:4317 \
-p 4318:4318 \
otel/opentelemetry-collector:latest \
--config=/etc/otel-collector-config.yaml


export LD_LIBRARY_PATH=$TMPDIR/otel-cpp/install/lib:$LD_LIBRARY_PATH
LD_PRELOAD=$TMPDEMODIR/libotel_preload.so ./ads_server

g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so     -I /mnt/otel/otel-cpp/install/include     -L /mnt/otel/otel-cpp/install/lib     -l opentelemetry_exporter_otlp_grpc     -l opentelemetry_sdk     -l opentelemetry_trace     - ldl -lpthread

g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so     -I/home/dragon/otel-cpp/install/include     -L/home/dragon/otel-cpp/install/lib     -lopentelemetry_exporter_otlp_grpc     -lopentelemetry_trace     -ldl -lpthread

















