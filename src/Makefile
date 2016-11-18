h_files := $(wildcard *.h)
cc_files := kdtree_main.cc
opts :=
# opts += -O3
opts += -Wall
opts += -std=c++14
opts += -g
ifdef PARALLEL
opts += -DPARALLEL
endif

CXXFLAGS := $(opts)
kdtree_main : $(cc_files) $(h_files)
	$(CXX)  -o $@ $(CXXFLAGS) $(cc_files) -lmyth

clean :
	rm -f kdtree_main
