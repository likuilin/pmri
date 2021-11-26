BzTree paper https://dl.acm.org/doi/10.1145/3164135.3164147

It uses PWmCAS and references this one https://ieeexplore.ieee.org/document/8509270

# Setup

Deps: `sudo apt-get install cmake clang-3.8 build-essential llvm libgtest-dev libgoogle-glog-dev libnuma-dev`

* `git clone --recursive https://github.com/microsoft/pmwcas` to get pmwcas (pmwcas directory is expected and gitignored at root of this repo)
* Build pmwcas: (see https://github.com/microsoft/pmwcas)
* `cd pmwcas` and `mkdir build` and `cd build`
* `cmake -DPMEM_BACKEND=Emu -DCMAKE_BUILD_TYPE=RelWithDebInfo ..`
* `make -j8`
* Each reboot: `echo 4096 | sudo tee /proc/sys/vm/nr_hugepages` to enable huge pages*
* Test pwmcas by doing `./mwcas_shm_server` and then in a separate terminal `./mwcas_tests`, if it works, yay
* Go back to root and `make` to make this project

*Note: This pins 8GB of RAM (2MB per huge page by default) and at least 4GB is required per shim server. If the host has less than 4GB RAM, try changing this: https://github.com/microsoft/pmwcas/blob/e9b2ba45257ff4e4b90c42808c068f3eda2f503d/src/environment/environment_linux.h#L109
