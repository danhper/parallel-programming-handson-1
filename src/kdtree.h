/*
 * kdtree.h
 */

#pragma once
#include <math.h>
#include <mtbb/task_group.h>
#include "kdtree_base.h"
#include "algorithm.h"

#define EPSILON 0.000000001

/* tree building procedures */

enum {
  /* maximum number of points in a leaf node */
  max_points_in_leaf = 10,
  /* the number of samples to choose a pivot from */
  n_samples_to_pivot = 5
};

/* find the pivot from points[a:b]
   that is likely to be near the median
   value of dth coordinates.
   simply sample n_samples_to_pivot points
   and choose the one that has the
   median of them.
 */
long find_piv(point * points, long a, long b, int d) {
  long idx[n_samples_to_pivot];
  int i, j;
  /* uniformly select ns indexes from [a,b) */
  for (i = 0; i < n_samples_to_pivot; i++) {
    idx[i] = a + (i * (b - a)) / n_samples_to_pivot;
  }
  /* selection-sort the n_samples_to_pivot elements
     by their dth coordinates */
  for (i = 0; i < (n_samples_to_pivot + 1) / 2; i++) {
    /* pick the minimum among [i,ns) */
    int m = i;
    for (j = i + 1; j < n_samples_to_pivot; j++) {
      /* ns - i - 1 */
      if (points[idx[j]].x[d] < points[idx[m]].x[d]) {
	m = j;
      }
    }
    long t = idx[i];
    idx[i] = idx[m];
    idx[m] = t;
  }
  return idx[(n_samples_to_pivot - 1) / 2];
}

/* partition
   points[a:b] into three parts.
   (i) the pivot
   (ii) points "left" of the pivot (along the dth axis)
   (iii) points "right" of the pivot (along the dth axis)
   the pivot moves to points[a],
   followed by all points in (ii)
   and then by all points in (iii).
 */
long partition(point * points, long a, long b, int d) {
  long piv_idx = find_piv(points, a, b, d);
  /* swap points[0] and pivot */
  point piv = points[piv_idx];
  points[piv_idx] = points[a];
  points[a] = piv;
  real piv_x = piv.x[d];
  long h, i;
  /* .....h....i.....
     <<<<<>>>>>?????? */
  for (h = a + 1, i = a + 1; i < b; i++) {
    if (points[i].x[d] < piv_x) {
      point t = points[h];
      points[h] = points[i];
      points[i] = t;
      h++;
    }
  }
  return h;
}

int compare_real(real a, real b)
{
    real diff = a - b;
    if (diff < EPSILON) {
        return -1;
    } else if (diff > EPSILON) {
        return 1;
    } else {
        return 0;
    }
}

long point_parallel_partition(point * points, long left, long right, long axis)
{
    long pivot_index = find_piv(points, left, right, axis);
    real pivot_value = points[pivot_index].x[axis];
    return parallel_partition<point, real>(points, pivot_value, left, right - 1,
                                           [axis](point p, real r) { return compare_real(p.x[axis], r); });
}

/* build kdtree from points[a:b].
   memory is allocated from nodes array,
   which can accommodate (at least) (b - a) nodes.
   b gives a bounding box of all points in points[a:b].
   split_axis is an integer (0 <= split < dim),
   which indicates along which axis to split the points.
 */
node * kdtree_build_rec(point * points, node * nodes,
			long a, long b, bbox box, int split_axis) {
  node * n = &nodes[a];
  n->p = points;
  n->a = a;
  n->b = b;
  n->box = box;
  if (b - a <= max_points_in_leaf) {
    n->split_axis = -1;
    /* redundant; just to avoid confusion */
    n->c[0] = n->c[1] = 0;
    return n;
  } else {
    /* h = index of the first point in the right partition.
       i.e.,
       points[a+1:h] is the points geometrically left of (or below) pivot
       points[h:b] is the points geometrically right of (or above) pivot */
    long h = point_parallel_partition(points, a, b, split_axis);
    // long h = partition(points, a, b, split_axis);
    int next_split_axis = (split_axis + 1) % dim;
    /* bounding boxes for children */
    bbox cbox[2] = { box, box };
    cbox[0].u.x[split_axis] = points[a].x[split_axis];
    cbox[1].l.x[split_axis] = points[a].x[split_axis];
    n->split_axis = split_axis;
    mtbb::task_group tg;
    tg.run([&] { n->c[0] = kdtree_build_rec(points, nodes, a + 1, h, cbox[0], next_split_axis); });
    n->c[1] = kdtree_build_rec(points, nodes, h,     b, cbox[1], next_split_axis);
    tg.wait();
    return n;
  }
}

node * kdtree_build(point * points, long n, bbox box) {
  node * nodes = new node[n];
  return kdtree_build_rec(points, nodes, 0, n, box, 0);
}


int kdtree_main(int argc, char ** argv) {
  /* number of points to index in kdtree */
  long n = (argc > 1 ? atol(argv[1]) : (1 << 22));
  /* number of times to repeat */
  long repeat = (argc > 2 ? atol(argv[2]) : 3);
  /* number of points to check the kdtree against */
  long n_checks = (argc > 3 ? atol(argv[3]) : 100);
  /* seed to generate the n points to index */
  long seed0 = (argc > 4 ? atol(argv[4]) : 775665918729);
  /* seed to generate the s points to check */
  long seed1 = (argc > 5 ? atol(argv[5]) : 665775729918);
  /* bounding box (0,0) -- (1,1) */
  bbox box = { { 0.0, 0.0 }, { 1.0, 1.0 } };
  /* generate n points */
  unsigned short mask = (1 << 16) - 1;
  unsigned short rg[3] = {
    (unsigned short)((seed0 >> 32) & mask),
    (unsigned short)((seed0 >> 16) & mask),
    (unsigned short)((seed0 >>  0) & mask),
  };
  point * points_org = mk_random_points(rg, n, box);
  /* the buffer to copy points_org to repeat the
     same computation */
  point * points = new point[n];
  node * nodes = new node[n];

  int r;
  /* repeat building kdt tree for the same points */
  for (r = 0; r < repeat; r++) {
    memcpy(points, points_org, sizeof(point) * n);
    memset(nodes, 0, sizeof(node) * n);
    printf("------ %d ------\n", r);
    printf("building kdtree of %ld points ... ", n); fflush(stdout);
    double t0 = cur_time();
    unsigned long long c0 = rdtscp();
    node * kdt = kdtree_build(points, n, box);
    unsigned long long c1 = rdtscp();
    double t1 = cur_time();
    double dt = t1 - t0;
    unsigned long long dc = c1 - c0;
    double nlogn = (double)n * log(n) / log(2);
    printf("done\n");
    if (r == 0) {
      if (n > 5000) {
	printf(" svn file not written as it would be too big\n");
      } else {
	printf(" writing svg file tree.svg ...\n");
	FILE * wp = fopen("tree.svg", "wb");
	if (!wp) { perror("fopen"); exit(1); };
	svg_kdtree(kdt, 500, wp);
	fclose(wp);
      }
    }
    printf("checking kdtree with %ld random points ... ", n_checks);
    fflush(stdout);
    int ec = check_kdtree(kdt, points, n, box, n_checks, seed1);
    if (ec == 0) {
      printf("OK\n");
      printf("built kdtree of %ld points in %f sec, %llu clocks\n",
	     n, dt, dc);
      printf(" time   = %f * n log(n) nano seconds\n",
	     1.0e9 * dt / nlogn);
      printf(" clocks = %f * n log(n) clocks\n",
	     dc / nlogn);
      printf(" n * log(n) / sec = %f x 10^9\n", 1.0e-9 * nlogn / dt);
      printf(" n * log(n) / clock = %f\n", nlogn / dc);
    } else {
      printf("NG (%d errors)\n", ec);
    }
  }
  return 0;
}
