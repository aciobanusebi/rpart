/* SCCS @(#)rpart.c	1.6 02/08/98 */
/*
** The main entry point for recursive partitioning routines.
**
** Input variables:
**      n       = # of observations
**      nvarx   = # of columns in xmat
**      ncat    = # categories for each var, 0 for continuous variables.
**      method  = 1 - anova
**                2 - exponential survival
**      node    = minimum node size
**      split   = minimum size of a node to attempt a split
**      maxpri  = max number of primary variables to retain (must be >0)
**      maxsur  = max number of surrogate splits to retain
**      usesur  = 0 - don't use the surrogates to do splits
**              = 1 - use them, but not the "go with majority" surrogate
**              = 2 - use surrogates fully
**      parms   = extra parameters for the split function, e.g. poissoninit
**      ymat    = matrix or vector of response variables
**      xmat    = matrix of continuous variables
**      missmat = matrix that indicates missing x's.  1=missing.
**      complex = initial complexity parameter
**      cptable  = a pointer to the root of the complexity parameter table
**      tree     = a pointer to the root of the tree
**      error    = a pointer to an error message buffer
**      xvals    = number of cross-validations to do
**      xgrp     = indices for the cross-validations
**
** Returned variables
**      error    = text of the error message
**      which    = final node for each observation
**
** Return value: 0 if all was well, 1 for error
**
*/
#include  <setjmp.h>
#include <stdio.h>
#include "rpart.h"
#include "node.h"
#include "func_table.h"
#include "rpartS.h"
#include "rpartproto.h"

int rpart(int n,         int nvarx,      long *ncat,     int method, 
          int mnode,     int msplit,     int  maxpri,    int maxsur,
	  int usesur,    double *parms,  double *ymat,   double *xmat,  
          long *missmat, double complex, struct cptable *cptable,
	  struct node **tree,            char **error,   int *which,
	  int xvals,     long *x_grp)
    {
    int i,j,k;
    int maxcat;
    double temp;

    /*
    ** Memory allocation errors from subroutines come back here
    */
    if (j=setjmp(errjump)) {
	*error = "Out of memory, cannot allocate needed structure";
	return(j);
	}

    /*
    ** initialize the splitting functions from the function table
    */
    if (method <= NUM_METHODS) {
	i = method -1;
	rp_init   = func_table[i].init_split;
	rp_choose = func_table[i].choose_split;
	rp_eval   = func_table[i].eval;
	rp_error  = func_table[i].error;
	rp.num_y  = func_table[i].num_y;
	}
    else {
	*error = "Invalid value for 'method'";
	return(1);
	}

    /*
    ** set some other parameters
    */
    rp.min_node =  mnode;
    rp.min_split = msplit;
    rp.complex = complex;
    rp.nvar = nvarx;
    rp.numcat = ncat;
    rp.maxpri = maxpri;
    if (maxpri <1) rp.maxpri =1;
    rp.maxsur = maxsur;
    rp.usesurrogate = usesur;
    rp.n = n;
    rp.which = which;

    /*
    ** create the "ragged array" pointers to the matrix
    **   x and missmat are in column major order
    **   y is in row major order
    */
    rp.xdata = (double **) ALLOC(nvarx, sizeof(double *));
    if (rp.xdata==0) longjmp(errjump, 1);
    for (i=0; i<nvarx; i++) {
	rp.xdata[i] = &(xmat[i*n]);
	}
    rp.ydata = (double **) ALLOC(n, sizeof(double *));
    if (rp.ydata==0) longjmp(errjump, 1);
    for (i=0; i<n; i++)  rp.ydata[i] = &(ymat[i*rp.num_y]);

    /*
    ** allocate some scratch
    */
    rp.tempvec = (int *)ALLOC(n, sizeof(int));
    rp.xtemp = (double *)ALLOC(n, sizeof(double));
    rp.ytemp = (double **)ALLOC(n, sizeof(double *));
    if (rp.tempvec==0 || rp.xtemp==0 || rp.ytemp==0) longjmp(errjump, 1);

    /*
    ** create a matrix of sort indices, one for each continuous variable
    **   This sort is "once and for all".  The result is stored on top
    **   of the 'missmat' array.
    ** I don't have to sort the categoricals.
    */
    rp.sorts  = (long**) ALLOC(nvarx, sizeof(long *));
    maxcat=0;
    for (i=0; i<nvarx; i++) {
	rp.sorts[i] = &(missmat[i*n]);
	for (k=0; k<n; k++) {
	    if (rp.sorts[i][k]==1) {
		rp.tempvec[k] = -(k+1);
		rp.xdata[i][k]=0;   /*weird numerics might destroy 'sort'*/
		}
	    else                   rp.tempvec[k] =  k;
	    }
	if (ncat[i]==0)  mysort(0, n-1, rp.xdata[i], rp.tempvec);
	else if (ncat[i] > maxcat)  maxcat = ncat[i];
	for (k=0; k<n; k++) rp.sorts[i][k] = rp.tempvec[k];
	}

    /*
    ** And now the last of my scratch space
    */
    if (maxcat >0) {
	rp.csplit = (int *) ALLOC(3*maxcat, sizeof(int));
	if (rp.csplit==0) longjmp(errjump, 1);
	rp.left = rp.csplit + maxcat;
	rp.right= rp.left   + maxcat;
	}
    else rp.csplit = (int *)ALLOC(1, sizeof(int));

    /*
    ** initialize the top node of the tree
    */
    for (i=0; i<n; i++) which[i] =1;
    i = rp_init(n, rp.ydata, maxcat, error, parms, &rp.num_resp, 1);
    nodesize = sizeof(struct node) + (rp.num_resp-2)*sizeof(double);
    *tree = (struct node *) calloc(1, nodesize);
    (*tree)->num_obs = n;
    if (i>0) return(i);

    (*rp_eval)(n, rp.ydata, (*tree)->response_est, &((*tree)->risk));
    (*tree)->complexity = (*tree)->risk;
    rp.alpha = rp.complex * (*tree)->risk;

    /*
    ** Do the basic tree
    */
    partition(1, (*tree), &temp);
    if ((*tree)->rightson ==0) {
	*error = "No splits could be created";
	return(1);
	}

    cptable->cp = (*tree)->complexity;
    cptable->risk = (*tree)->risk;
    cptable->nsplit = 0;
    cptable->forward =0;
    cptable->xrisk =0;
    cptable->xstd =0;
    rp.num_unique_cp =1;
    make_cp_list((*tree), (*tree)->complexity, cptable);
    make_cp_table((*tree), (*tree)->complexity, 0);

    if (xvals >1) xval(xvals, cptable, x_grp, maxcat, error, parms);
    /*
    ** all done
    */
    return(0);
    }
