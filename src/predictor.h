//========================================================//
//  predictor.h                                           //
//  Header file for the Branch Predictor                  //
//                                                        //
//  Includes function prototypes and global predictor     //
//  variables and defines                                 //
//========================================================//

#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <stdint.h>
#include <stdlib.h>


//------------------------------------//
//      Global Predictor Defines      //
//------------------------------------//
#define NOTTAKEN  0
#define TAKEN     1

// The Different Predictor Types
#define STATIC      0
#define GSHARE      1
#define TOURNAMENT  2
#define TAGE        3
extern const char *bpName[];

// Definitions for 2-bit counters
#define SN  0			// predict NT, strong not taken
#define WN  1			// predict NT, weak not taken
#define WT  2			// predict T, weak taken
#define ST  3			// predict T, strong taken

//Definitions for selector in tournament predictor
#define SG  0           // strongly select global predictor
#define WG  1           // weakly select global predictor 
#define WL  2           // weakly select local predictor
#define SL  3           // strongly select local predictor

//Define useful counter in TAGE
#define S_UN 0
#define W_UN 1
#define W_US 2
#define S_US 3
//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//
extern int ghistoryBits; // Number of bits used for Global History
extern int lhistoryBits; // Number of bits used for Local History
extern int pcIndexBits;  // Number of bits used for PC index
extern int bpType;       // Branch Prediction Type
extern int verbose;

//------------------------------------//
//    Predictor Function Prototypes   //
//------------------------------------//

// Initialize the predictor
//
void init_predictor();

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc);

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint8_t outcome);

//gshare
_Bool gshare_predict(uint32_t pc);
void gshare_train(uint8_t outcome);

//tournament
_Bool tournament_predict(uint32_t pc);
void tournament_train(uint8_t outcome);

//TAGE
_Bool TAGE_predict(uint32_t pc);
void TAGE_train(uint32_t pc,uint8_t outcome);
uint32_t TAGE_index_hash(uint64_t GHR,uint32_t pc,uint64_t GHR_mask,uint32_t table_num);
uint32_t TAGE_tag_hash(uint64_t GHR,uint32_t pc,uint64_t GHR_mask,uint32_t table_num);
uint32_t rand_allocate(uint32_t* avail_tables, int avail_num);

int INTpow(int base,int pow);

#endif
