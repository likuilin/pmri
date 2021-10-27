# Notes

BzTree paper https://dl.acm.org/doi/10.1145/3164135.3164147

It uses PWmCAS and references this one https://ieeexplore.ieee.org/document/8509270

Of which implementation is on github, so we can just borrow their PWmCAS and make BzTree ourselves using it? https://github.com/microsoft/pmwcas

(Or we can re-implement PWmCAS but eh.)

== Trying to use their pwmcas:

Install (ubuntu): `sudo apt install libgtest-dev libgoogle-glog-dev libnuma-dev` (and probably others, these were the extra ones I needed though)

* Check out
* `git clone --recursive https://github.com/microsoft/pmwcas` to get pmwcas
* Build pmwcas: (see https://github.com/microsoft/pmwcas)
* `cd pmwcas` and `mkdir build` and `cd build`
* `cmake -DPMEM_BACKEND=Emu -DCMAKE_BUILD_TYPE=RelWithDebInfo ..`
* `make -j8`

Segfault on dereferencing 0xffffffffffffffff? Sigh

I am trying to work if we can still use their PWmCAS.

=== Giving up, let's re-implement PWmCAS!

