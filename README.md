BzTree paper https://dl.acm.org/doi/10.1145/3164135.3164147

It uses PMwCAS and references this one https://ieeexplore.ieee.org/document/8509270

For simplicity of building, we just fork pmwcas and build our application inside it.

# Setup

Deps: `sudo apt-get install cmake build-essential llvm libgtest-dev libgoogle-glog-dev libnuma-dev libpmemobj-dev`

To get libpmemobj.so in the right place: `sudo ln -s $(dpkg -L libpmemobj-dev | grep "/libpmemobj.so") /usr/local/lib/libpmemobj.so`

* `git submodule update --init --recursive` to get pmwcas deps
* `mkdir build` and `cd build`
* `cmake -DPMEM_BACKEND=PMDK -DCMAKE_BUILD_TYPE=RelWithDebInfo ..`
* `make -j8`
* Each reboot: `echo 4096 | sudo tee /proc/sys/vm/nr_hugepages` to reserve necessary huge pages*
* Test pmwcas by doing `./mwcas_shm_server` and then in a separate terminal `./mwcas_tests`, if it works, yay
* Our code is in `src/bztree` and the test builds to `./bztree_tests`. The shim server must be running for the tests.

*Note: This pins 8GB of RAM (2MB per huge page by default) and at least 4GB is required per shim server. If the host has less than 4GB RAM, try changing `kNumaMemorySize` in `src/environment/environment_linux.h`. After you're done working on this, unpin the memory by doing `echo 0 | sudo tee /proc/sys/vm/nr_hugepages`
