#!/bin/bash

for p in 1 2 4 6 8 12 16 24 32 40 48 56 64 ; do
  echo "Running with $p cores"
  MYTH_NUM_WORKERS=${p} ./kdtree_main 4194304 3 30
done
