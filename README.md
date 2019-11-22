# Branch Predictor Project

## Table of Contents
  * [Introduction](#introduction)
  * [Acknowledgment](#acknowledgment)
  * [Traces](#traces)
  * [Running predictors](#running-predictors)
  * [Implementing the predictors](#implementing-the-predictors)
    - [Gshare](#gshare)
    - [Tournament](#tournament)
    - [TAGE](#TAGE)
    - [Things to note](#things-to-note)


## Introduction

This is the branch predictor (BP) project of graduate computer architecture course at UCSD. If you are currently taking graduate computer architecture at UCSD and doing this project, for the academic integrity consideration, please don't check and use codes in this repository. However, you are more than welcomed to use the reference documentations below. 

Three BPs are implemented here:

1. gshare: Proposed by **McFarling** in *Combining branch predictors*. Technical Report TN-36, Vol. 49, Digital Western Research Laboratory. Original paper link: http://classweb.ece.umd.edu/enee646.F2007/combining.pdf.

2. tournament: Popularized by Alpha 21264. The original paper should be this one (Not 100% sure): https://www.cs.tufts.edu/comp/150PAT/arch/alpha/leibholz.pdf. But the detail of tournament BP is introduced clearly in that paper, some helpful details are presented in this document: http://courses.cs.tamu.edu/ejkim/614/tournament_predictors.pdf.

3. TAGE: Proposed by **Seznec Andre** in *A case for (partially)-tagged geometric history length predictors*. Journal of InstructionLevel Parallelism, Vol. 8, 2006. It's still one of state of the art branch predictors even if it's proposed over decade ago. Original paper link: https://www.jilp.org/vol8/v8paper1.pdf.

## Acknowledgment
Traces and start code are provided by TA of this courses: Prannoy Pilligundla. 

## Traces

These predictors will make predictions based on traces of real programs.  Each line in the trace file contains the address of a branch in hex as well as its outcome (Not Taken = 0, Taken = 1):

```
<Address> <Outcome>
Sample Trace from int_1:

0x40d7f9 0
0x40d81e 1
0x40d7f9 1
0x40d81e 0
```

## Running predictors

To run predictors, firstly run `make` in the src/ directory of the project.  Then run the generated `predictor` executable program. To run the predictor on uncompressed trace file, use the following command:   

`./predictor <options> [<trace>]`

Predictors can also be run on a compressed trace by using the following command:

`bunzip2 -kc trace.bz2 | ./predictor <options>`

If no trace file is provided then the predictor will read in input from STDIN.

In either case the `<options>` that can be used to change the type of predictor being run are as follows:

```
  --help       Print usage message
  --verbose    Outputs all predictions made by your
               mechanism. Will be used for correctness
               grading.
  --<type>     Branch prediction scheme. Available
               types are:
        static
        gshare:<# ghistory>
        tournament:<# ghistory>:<# lhistory>:<# index>
        TAGE
```
An example of running a gshare predictor with 10 bits of history would be:   

`bunzip2 -kc ../traces/int1_bz2 | ./predictor --gshare:10`


## Implementing the predictors

There are 3 parts to be implemented in the predictor.c file in order to implement a BP.
They are: **init_predictor**, **make_prediction**, and **train_predictor**.

`void init_predictor();`

This will be run before any predictions are made.  This is where initialization of any data structures or values for a particular branch predictor 'bpType'.  All switches will be set prior to this function being called.

`uint8_t make_prediction(uint32_t pc);`

PC of a branch is given as the input of this function and BP will make a prediction of TAKEN or NOTTAKEN as the return value of this function. 

`void train_predictor(uint32_t pc, uint8_t outcome);`

Once a prediction is made a call to train_predictor will be made. Update of any relevant data structures based on the true outcome of the branch is done in this function. 

#### Gshare

```
Configuration:
    ghistoryBits    // Indicates the length of Global History kept
```
The Gshare predictor is characterized by XORing the global history register with the lower bits (same length as the global history) of the branch's address.  This XORed value is then used to index into a 1D BHT of 2-bit predictors.

#### Tournament
```
Configuration:
    ghistoryBits    // Indicates the length of Global History kept
    lhistoryBits    // Indicates the length of Local History kept in the PHT
    pcIndexBits     // Indicates the number of bits used to index the PHT
```

The difference between the Alpha 21264's predictor and the one implemented here is that all of the underlying counters here are 2-bit predictors. (In accordance with the requirement of the course project). The 'ghistoryBits' will be used to size the global and choice predictors while the 'lhistoryBits' and 'pcIndexBits' will be used to size the local predictor.

#### TAGE

The parameters of TAGE are a little bit more, so currently it's hard coded in predictor.c file. It will be added to input arguments in later updates. 

Main parameters include PC bits used in basic predictor, number of partial table (or partial predictor), global history bits used in each partial 
table, size of each partial table.

#### Things to note

All history are initialized to NOTTAKEN.  History registers are updated by shifting in new history to the least significant bit position.
```
Ex. 4 bits of history, outcome of next branch is NT
  T NT T NT   <<  NT
  Result: NT T NT NT
```
```
All 2-bit predictors are initialized to WN (Weakly Not Taken).
They should also have the following state transitions:

        NT      NT      NT
      ----->  ----->  ----->
    ST      WT      WN      SN
      <-----  <-----  <-----
        T       T       T
```

The Choice Predictor used to select which predictor to use in the Alpha 21264 Tournament predictor are initialized to Weakly select the Global Predictor.
