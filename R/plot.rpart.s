# @(#)plot.rpart.s	1.5 03/12/97
plot.rpart <- function(tree, uniform=F, branch=1, compress=F, 
			     nspace, margin=0, minbranch=.3, ...){
    if(!inherits(tree, "rpart"))
	    stop("Not an rpart object")

    if (compress & missing(nspace)) nspace <- branch
    if (!compress) nspace <- -1     #means no compression
    assign(paste(".rpart.parms", dev.cur(), sep = "."), 
            list(uniform=uniform, branch=branch, nspace=nspace,
		 minbranch=minbranch), envir=.GlobalEnv)

    #define the plot region
    temp <- rpartco(tree)
    x <- temp$x
    y <- temp$y
    temp1 <- range(x) + diff(range(x))*c(-margin, margin)
    temp2 <- range(y) + diff(range(y))*c(-margin, margin)
    plot(temp1, temp2, type='n', axes=F, xlab='', ylab='', ...)

    # Draw a series of horseshoes or V's, left son, up, down to right son
    #   NA's in the vector cause lines() to "lift the pen"
    node <- as.numeric(row.names(tree$frame))
    temp <- rpart.branch(x, y, node, branch)
	  
    if (branch>0) text(x[1], y[1], '|')
    lines(c(temp$x), c(temp$y))
    invisible(list(x=x, y=y))
}





