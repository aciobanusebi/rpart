# SCCS @(#)rpart.matrix.s	1.3 01/21/97
#
# This differs from tree.matrix in xlevels -- we don't keep NULLS in
#   the list for all of the non-categoricals
#
rpart.matrix <- function(frame)
    {
    if(!inherits(frame, "data.frame"))
	    return(as.matrix(frame))
    frame$"(weights)" <- NULL
    terms <- attr(frame, "terms")
    if(is.null(terms)) predictors <- names(frame)
    else predictors <- as.character(attr(terms, "term.labels"))
    frame <- frame[predictors]
#     else {
#       a <- attributes(terms)
#       predictors <- as.character(a$variables)[-1]
#       removals <- NULL
#       if ((TT <- a$response) > 0) {
#         removals <- TT
#       }
#       if (!is.null(TT <- a$offset)) {
#         removals <- c(removals, TT)
#       }
#       if (!is.null(removals)) {
#         predictors <- predictors[-removals]
#         frame <- frame[, -removals]
#       }
#     }

    factors <- sapply(frame, function(x) !is.null(levels(x)))
    characters <- sapply(frame, is.character)
    if(any(factors | characters)) {
	# change characters to factors
	for (preds in predictors[characters])
		frame[preds] <- as.factor(frame[preds])
        factors <- factors | characters
        column.levels <- lapply(frame[factors], levels)
	names(column.levels) <- (1:ncol(frame))[factors]

	# Now make them numeric
	for (preds in predictors[factors])
	     frame[[preds]] <- as.numeric(frame[[preds]])
	x <- as.matrix(frame)
	attr(x, "column.levels") <- column.levels
	}
    else x <- as.matrix(frame)
    class(x) <- "matrix"
    x
    }


