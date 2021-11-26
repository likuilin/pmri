CXX = clang++
CXXFLAGS = -Wall -Wextra -O2 -g --std=c++11 -isystem pmwcas -isystem pmwcas/src
LDFLAGS = -lnuma "-L/home/kuilin/projects/pmri/pmwcas/build/gtest/src/GTestExternal-build" "-L/home/kuilin/projects/pmri/pmwcas/build/gtest/src/GTestExternal-build/googlemock/gtest" "-L/home/kuilin/projects/pmri/pmwcas/build/gflags/src/GFlagsExternal-build/lib" "-L/home/kuilin/projects/pmri/pmwcas/build/glog/src/GLogExternal-build" "-Wl,-rpath,/home/kuilin/projects/pmri/pmwcas/build/gtest/src/GTestExternal-build:/home/kuilin/projects/pmri/pmwcas/build/gtest/src/GTestExternal-build/googlemock/gtest:/home/kuilin/projects/pmri/pmwcas/build/gflags/src/GFlagsExternal-build/lib:/home/kuilin/projects/pmri/pmwcas/build/glog/src/GLogExternal-build:/home/kuilin/projects/pmri/pmwcas/build" "-lpthread" "-lrt" "-lnuma" "-lglog" "-lgflags" "-lgtest" "-lpthread" "-lrt" "-lnuma" "-rdynamic"

LIBPMWCAS = pmwcas/build/libpmwcas.so

main: main.o $(LIBPMWCAS)
	$(CXX) $(LDFLAGS) $(LIBPMWCAS) main.o -o main

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

clean:
	rm -f main.o main

.PHONY: clean

$(LIBPMWCAS):
	$(error Error: Expected $(LIBPMWCAS) to exist. See the readme to build libpmwcas)
