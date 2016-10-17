/* 
 * kdtree.h
 */

#pragma once
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* dimension */
enum { dim = 2 };

typedef float real;
typedef unsigned long long ts_t; /* time stamp */

/* point */
typedef struct {
  real x[dim];
} point;

/* bounding box */
typedef struct {
  /* 
     +-----+ u
     |     |
   l +-----+
   */
  point l;			/* lower left corner */
  point u;			/* upper right corner */
} bbox;

/* node */
typedef struct node {
  point * p;  /* pointer to the global array of points (same for all nodes) */
  long a, b;  /* my range (i.e., p[a:b] is the points in this node) */
  bbox box;   /* bounding box of this node */
  int depth;  /* depth from the root (root's depth is zero) */
  int split_axis; /* axis along which this node is split
		     (an integer 0 ... dim-1, or -1 for leaf) */
  struct node * c[2]; /* the two children (each may be null) */
} node;

/* utilities */

/* time measurement */

/* read timestamp counter */
static inline ts_t rdtscp() {
  ts_t u;
  asm volatile ("rdtsc; shlq $32,%%rdx;"
		" orq %%rdx,%%rax;"
		" movq %%rax,%0"
		: "=q"(u) : : "%rax", "%rdx", "%rcx");
  return u;
}

/* get time by syscall */
double cur_time() {
  struct timeval tp[1];
  gettimeofday(tp, 0);
  return tp->tv_sec + 1.0e-6 * tp->tv_usec;
}

/* data preparation.
   make a random point in bounding box l, u */
point mk_random_point(unsigned short * rg, bbox box) {
  int i;
  point p;
  for (i = 0; i < dim; i++) {
    p.x[i] = box.l.x[i] + (box.u.x[i] - box.l.x[i]) * erand48(rg);
  }
  return p;
}

/* data preparation.
   make n random points, all in bounding box l, u */
point * mk_random_points(unsigned short * rg, long n, bbox box) {
  point * p = new point[n];
  long i;
  for (i = 0; i < n; i++) {
    p[i] = mk_random_point(rg, box);
  }
  return p;
}

/* write point into string buffer s,
   like this.
   "0.123456 0.789123 0.456789" */
long snprint_point(char * s, long n, point p) {
  long w = 0;			/* bytes written */
  int i;
  int x;
  for (i = 0; i < dim; i++) {
    if (i > 0) {
      assert(w < n);
      s[w] = 0;
      x = snprintf(s + w, n - w, " ");
      assert(x == 1);
      w += x;
    }
    assert(w < n);
    s[w] = 0;
    x = snprintf(s + w, n - w, "%f", p.x[i]);
    assert((size_t)x == strlen(s + w));
    w += x;
  }
  return w;
}

/* print n points */
void print_points(point * p, long n) {
  long i;
  for (i = 0; i < n; i++) {
    char s[100];
    snprint_point(s, 100, p[i]);
    printf("%s\n", s);
  }
}

/* print point in svg format */
void svg_point(point p, real scale, FILE * wp) {
  int r = fprintf(wp,
		  "<circle cx=\"%d\" cy=\"%d\" r=\"4\""
		  " stroke=\"none\" fill=\"red\"/>\n",  
		  (int)(p.x[0] * scale), (int)(p.x[1] * scale));
  if (r < 0) { perror("fprintf"); exit(1); }
}

/* print bounding box (rectangle) in svg */
void svg_bb(bbox box, real scale, FILE * wp) {
  int r = fprintf(wp, 
		  "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
		  "stroke=\"black\" fill=\"none\"/>\n",
		  (int)(box.l.x[0] * scale), (int)(box.l.x[1] * scale), 
		  (int)((box.u.x[0] - box.l.x[0]) * scale), 
		  (int)((box.u.x[1] - box.l.x[1]) * scale));
  if (r < 0) { perror("fprintf"); exit(1); }
}

/* print an svg representation of a kdtree rooted at node n, into file wp */
void svg_kdtree_rec(node * n, real scale, FILE * wp) {
  if (n) {
    svg_bb(n->box, scale, wp); /* write the enclosing box */
    if (n->split_axis == -1) {
      /* print its points for leaves */
      long i;
      for (i = n->a; i < n->b; i++) {
	svg_point(n->p[i], scale, wp);
      }
    } else {
      /* print its (only) point for non-leaves */
      svg_point(n->p[n->a], scale, wp);
      int i;
      /* and recursively draw children */
      for (i = 0; i < 2; i++) {
	svg_kdtree_rec(n->c[i], scale, wp);
      }
    }
  }
}

/* write a kdtree rooted at n, as a standalone svg file (with headers) */
void svg_kdtree(node * n, real scale, FILE * wp) {
  int r = fprintf(wp,
		  "<?xml version=\"1.0\" encoding=\"UTF-8\""
		  " standalone=\"yes\"?>\n");
  if (r < 0) { perror("fprintf"); exit(1); }
  r = fprintf(wp,
	      "<svg xmlns=\"http://www.w3.org/2000/svg\""
	      " width=\"%d\" height=\"%d\" viewBox=\"%d %d %d %d\" >",
	      (int)scale, (int)scale, 0, 0, (int)scale, (int)scale);
  if (r < 0) { perror("fprintf"); exit(1); }
  svg_kdtree_rec(n, scale, wp);
  r = fprintf(wp, "</svg>\n");
  if (r < 0) { perror("fprintf"); exit(1); }
}

/* search related procedures
   (still nothing to do with building tree) */

/* squared distance between two points p and q */
real dist2(point p, point q) {
  real d2 = 0;
  int i;
  for (i = 0; i < dim; i++) {
    real dx = q.x[i] - p.x[i];
    d2 += dx * dx;
  }
  return d2;
}

/* the minimum squared distance between a point and a rectangle.
                                           p
                                          /
                                         /
      +-------+    +-------+    +-------+
 p ---+       |    |       |    |       |
      |   b   |    |   b   |    |   b   |
      |       |    |       |    |       |
      +-------+    +-------+    +-------+
                      |                  
                      |
                      p
*/

real dist2_bb(point p, bbox box) {
    real d2 = 0;
    int i;
    for (i = 0; i < dim; i++) {
      real px = p.x[i];
      real lx = box.l.x[i];
      real ux = box.u.x[i];
      assert(ux > lx);
      if (px < lx) {
	d2 += (lx - px) * (lx - px);
      } else if (px > ux) {
	d2 += (px - ux) * (px - ux);
      }
    }
    return d2;
}

/* nearest neighbor search with a kdtree n.
   (not necessary to understand this to 
   build a kdtree).
   p is the closest point found so far,
   used to prune searching a subtree.
   if there is a node under n
   closer than p to c, return 
   the point nearest to c.
   otherwise return p */
point * kdtree_nn_search_rec(node * n, point c, point * p) {
  if (n) {
    if (n->split_axis == -1) {
      /* leaf : linear search all points, for
	 points closer than p */
      long i;
      for (i = n->a; i < n->b; i++) {
	/* check if this node is closer than m */
	if (!p || dist2(c, n->p[i]) < dist2(c, *p)) {
	  /* it is. n is closest so far */
	  p = &n->p[i];
	}
      }
    } else {
      /* this is an internal node;
	 first calc minimum dist to n's 
	 bounding box */
      real d2 = dist2_bb(c, n->box);
      /* if d2 is farther than p, done.
	 otherwise check its point 
	 and call children */
      if (!p || d2 < dist2(c, *p)) {
	int i;
	/* the point of n */
	if (!p || dist2(c, n->p[n->a]) < dist2(c, *p)) {
	  p = &n->p[n->a];
	}
	for (i = 0; i < 2; i++) {
	  p = kdtree_nn_search_rec(n->c[i], c, p);
	}
      }
    }
  }
  return p;
}

/* search kdtree kdt for the point closest to c */
point * kdtree_nn_search(node * kdt, point c) {
  return kdtree_nn_search_rec(kdt, c, 0);
}

/* search for the point closest to c in points[0:n] by a linear scan */
point * naive_nn_search(point * points, long n, point c) {
  point * p = 0;
  long i;
  for (i = 0; i < n; i++) {
    if (!p || dist2(c, points[i]) < dist2(c, *p)) {
      p = &points[i];
    }
  }
  return p;
}

/* check if a kdtree kdt, which has been built from points[0:n] 
   in bounding box bb,
   correctly returns the nearest point for randomly
   generated points.
   n_checks : the number of random points to be checked
   seed : the seed for a random number generator
 */
int check_kdtree(node * kdt, point * points, long n, bbox box,
		 long n_checks, unsigned long seed) {
  long i;
  unsigned short mask = (1 << 16) - 1;
  unsigned short rg[3] = { (unsigned short)((seed >> 32) & mask),
			   (unsigned short)((seed >> 16) & mask),
			   (unsigned short)((seed >>  0) & mask) };
  int ec = 0; /* error count */
  for (i = 0; i < n_checks; i++) {
    point c = mk_random_point(rg, box);
    point * x = kdtree_nn_search(kdt, c);
    point * y = naive_nn_search(points, n, c);
    if (x != y) {
      char cs[100], xs[100], ys[100];
      snprint_point(cs, 100, c);
      snprint_point(xs, 100, x[0]);
      snprint_point(ys, 100, y[0]);
      printf("ERROR:\n");
      printf("kdtree %s -> %s, dist2 = %f\n", cs, xs, dist2(x[0], c));
      printf("naive  %s -> %s, dist2 = %f\n", cs, ys, dist2(y[0], c));
      ec++;
    }
  }
  return ec;
}

