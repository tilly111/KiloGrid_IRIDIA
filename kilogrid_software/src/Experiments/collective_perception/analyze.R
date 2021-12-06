script.dir <- dirname(sys.frame(1)$ofile)

source(paste(script.dir, "../../scripts/process.R", sep="/"))

read.kilodat.original <- read.kilodat

read.kilodat <- function(filename) {
  df <- read.kilodat.original(filename)
  data.frame(df, state = df$data0, opinion = df$data1 + 1)
}


analyze.main <- function(filename, savePDF = F, interval = 1) {
  df <- read.kilodat(filename)
  dp <- analyze.transition.probability(df)

  if(savePDF) {
    pdfName <- paste(filename, "pdf", sep=".")
    pdf(pdfName)
  }

  num_opinions <- length(unique(df$opinion))
  data <- subset(dp, timestamp == 1 | timestamp %% (interval * TICKS_PER_SEC) == 0)
  for(o in 1:num_opinions) {
    plot(data$timestamp / TICKS_PER_SEC, data[,names(dp) == paste("opinion", o, sep="")],
         col=o+1, type="l", ylim=c(0, 1), ylab="P(opinion)", xlab="Time [sec]")
    par(new=T)
  }
  par(new=F)

  if(savePDF) {
    dev.off()
  }
}


analyze.first_pos_and_opinion <- function(df, plot=F) {
  sdf <- kilodat.divide.kilobot(df)
  fpo <- lapply(sdf,
                function(data){
                  data[order(data$timestamp),][1,c("x", "y", "opinion")]
                })
  ret <- Reduce(function(d1, d2){
                  data.frame(
                    x = c(d1$x, d2$x),
                    y = c(d1$y, d2$y),
                    opinion = c(d1$opinion, d2$opinion))
                }, fpo)
  if(plot) {
    plot(
      ret$x, ret$y, col = ret$opinion + 1,
      xlim = c(0,NUM_SUBCELLS_X), xlab = "X",
      ylim = c(0,NUM_SUBCELLS_Y), ylab = "Y"
    )
  }
}


analyze.transition.simple <- function(df) {
  ddf <- kilodat.divide.kilobot(df)
  ret <- lapply(ddf, function(data){ data[,c("timestamp", "opinion")]})
  for(data in ret) {
    plot(data$timestamp, data$opinion, type="p", xlim=c(0, max(df$timestamp)), ylim=c(0, max(df$opinion)))
    par(new=T)
  }
  par(new=F)
}


analyze.transition.probability <- function(df) {
  num_opinions <- length(unique(df$opinion))
  divided_df   <- kilodat.divide.kilobot(df)
  num_kilobots <- length(divided_df)
  end_t <- max(df$timestamp)
  # Generate transition matrix
  transitions <- kilodat.transitions(df, target=c("opinion"))
  t_mat <- matrix(ncol = num_kilobots, nrow = end_t)
  for(k in 1:num_kilobots) {
    t_mat[ , k] <- transitions[[k]]$opinion
  }
  # Generate probability matrix
  p_mat <- sapply(1:end_t,
                  function(t){
                    c(t, sapply(1:num_opinions,
                      function(o){
                        sum(t_mat[t, ] == o) / num_kilobots
                    }))
                  })
  # Convert into data frame
  ret <- as.data.frame(t(p_mat))
  names(ret) <- c("timestamp",
                  lapply(1:num_opinions,
                         function(o){ paste("opinion", o, sep="") }))
  ret
}
