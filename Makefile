h_files := $(wildcard *.h)
cc_files := $(wildcard *.cc)
opts :=
opts += -O3
opts += -Wall

CXXFLAGS := $(opts)
kdtree_main : $(cc_files) $(h_files)
	$(CXX) -std=c++0x -o $@ $(CXXFLAGS) $(cc_files) -lmyth

clean :
	rm -f kdtree_main
