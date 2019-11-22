//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"
#include <math.h>

//
// TODO:Student Information
//
const char *studentName = "Wang Pi";
const char *studentID   = "A53298021";
const char *email       = "wapi@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//--------Data Structure for gshare and tournament------------
uint32_t GHR; //Global history register, the shift direction is toward the higher bits of pc
uint32_t global_mask; //Bit operation global_mask, extract lower "ghistoryBits" bits from pc and GHR
uint8_t* BHT; //Branch history table in gshare, global history table in tournament predictor
uint32_t BHT_index; //Index to enter BHT
_Bool global_res;

//----------------Data Structure for tournament--------------
uint8_t* SHT; //Selection history in tournament predictor
uint8_t* local_BHT; //Global history table in tournament predictor
uint32_t* local_PHT; //Local pattern history table in tournament predictor
uint32_t local_pc_mask;
uint32_t local_PHT_mask;
uint32_t local_PHT_index;
uint32_t local_BHT_index;
_Bool local_res;

//-------------Data Structure for TAGE----------
#define PAR_TABLE_NUM 3  //Number of partial tables in TAGE 
#define TAGE_CNT_BITS 2  //Two bits counter in each entry of partial table
#define TAGE_TAG_BITS 0  //Bits of tag in each entry of partial table 
#define GHRLEN_COM_RATIO 2.1  //Common ratio of history length seqeuence
#define GHRLEN_MIN 11 //The global history length used in the first partial table
//GHRLEN_MIN*(GHRLEN_COM_RATIO)^(i-1) is the global history length used in ith table
//Need to gurantee (int) GHRLEN_MIN*(GHRLEN_COM_RATIO)^(PAR_TABLE_NUM-1) doesn't exceed 64, since uint64_t is used as GHR

#define TAGE_PC_BITS 13  //Bits of PC used to index BHT in basic predictor
uint32_t TAGE_PAR_INDEX_BITS[PAR_TABLE_NUM];  //Bits of result of index hash function, used to index each partial table


//Each entry of partial table include tag, saturating counter and whether useful flag
uint32_t* partial_table_TAG[PAR_TABLE_NUM];  
uint8_t* partial_table_BH[PAR_TABLE_NUM];
uint8_t* partial_table_USE[PAR_TABLE_NUM];

//Each time TAGE makes the prediction, it will compute hash index and hash tag for each partial table, store them to avoid recomputing in the following train stage
uint32_t partial_table_lastIndice[PAR_TABLE_NUM];
uint32_t partial_table_lastTags[PAR_TABLE_NUM];

uint64_t TAGE_GHR;
uint32_t TAGE_GHR_lenTable[PAR_TABLE_NUM];  //TAGE_GHR_lenTable[i]=GHRLEN_MIN*(GHRLEN_COM_RATIO)^(i-1)
uint64_t TAGE_GHR_mask[PAR_TABLE_NUM];  //Used to extract required length

uint8_t* TAGE_BHT;  //BHT of basic predictor
uint32_t TAGE_BHT_mask;  
uint32_t TAGE_BHT_index;  

//Store the table number, index and result of first choice and alternative choice in prediction to use in the train stage
int firstChoice_table;  
int alter_table; 
uint32_t firstChoice_index;
uint32_t alter_index;
_Bool firstChoice_res;
_Bool alter_res;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize Branch Predictor Data Structures
//
void init_predictor(){
	//Initialize global BHT and global global_mask for gshare and tournament
	GHR=0;
	uint32_t BHT_size=1;
  	for(int i=0;i<ghistoryBits;i++)
    	BHT_size*=2;
	BHT=(uint8_t*)malloc(BHT_size*sizeof(uint8_t));
	SHT=(uint8_t*)malloc(BHT_size*sizeof(uint8_t));
	for(int i=0;i<BHT_size;i++){
		BHT[i]=WN;
		SHT[i]=WG; //Initialize selectiion HT, useless in gshare
	} 
	global_mask=BHT_size-1;

	if(bpType==TOURNAMENT){

		//Initialize local PHT and local pc mask
		uint32_t local_PHT_size=1;
		for(int i=0;i<pcIndexBits;i++)
			local_PHT_size*=2;
		local_PHT=(uint32_t*)malloc(local_PHT_size*sizeof(uint32_t));
		for(int i=0;i<local_PHT_size;i++)
			local_PHT[i]=0;
		local_pc_mask=local_PHT_size-1;

		//Initialize local BHT and local PHT mask
		uint32_t local_BHT_size=1;
		for(int i=0;i<lhistoryBits;i++)
			local_BHT_size*=2;
		local_BHT=(uint8_t*)malloc(local_BHT_size*sizeof(uint8_t));
		for(int i=0;i<local_BHT_size;i++)
			local_BHT[i]=WN;
		local_PHT_mask=local_BHT_size-1;	

	}else if(bpType==CUSTOM){
		TAGE_GHR=0;

		TAGE_PAR_INDEX_BITS[0]=12;
		TAGE_PAR_INDEX_BITS[1]=12;
		TAGE_PAR_INDEX_BITS[2]=12;

		//Initialize GHR length and GHR mask of each partial table
		for(int i=0;i<PAR_TABLE_NUM;i++){
			if(0==i)
				TAGE_GHR_lenTable[i]=GHRLEN_MIN;
			else
				TAGE_GHR_lenTable[i]=(uint32_t) TAGE_GHR_lenTable[i-1]*GHRLEN_COM_RATIO;
			TAGE_GHR_mask[i]=(uint64_t) (INTpow(2,TAGE_GHR_lenTable[i])-1);
		}
		
		//Compute partial table and BHT size
		uint32_t par_table_size[PAR_TABLE_NUM];
		uint32_t TAGE_BHT_size=1;
		for(int i=0;i<PAR_TABLE_NUM;i++)
			par_table_size[i]=INTpow(2,TAGE_PAR_INDEX_BITS[i]);
		TAGE_BHT_size=INTpow(2,TAGE_PC_BITS);
		TAGE_BHT_mask=TAGE_BHT_size-1;

		//Initialize each partial table
		for(int i=0;i<PAR_TABLE_NUM;i++){
			partial_table_TAG[i]=(uint32_t*)malloc(par_table_size[i]*sizeof(uint32_t));
			partial_table_BH[i]=(uint8_t*)malloc(par_table_size[i]*sizeof(uint8_t));
			partial_table_USE[i]=(uint8_t*)malloc(par_table_size[i]*sizeof(uint8_t));
			for(int j=0;j<par_table_size[i];j++){
				partial_table_TAG[i][j]=0;
				partial_table_BH[i][j]=WN;
				partial_table_USE[i][j]=0;
			}
		}

		//Initialize BHT
		TAGE_BHT=(uint8_t*)malloc(TAGE_BHT_size*sizeof(uint8_t));
		for(int i=0;i<TAGE_BHT_size;i++)
			TAGE_BHT[i]=WN;
	}
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc){
  // Make a prediction based on the bpType
	switch (bpType) {
    	case STATIC:
      		return TAKEN;
    	case GSHARE:
			return gshare_predict(pc);
		case TOURNAMENT:
			// fprintf(stdout,"The result is: %u\n",tournament_predict(pc));
			return tournament_predict(pc);
		case CUSTOM:
			return TAGE_predict(pc);
		default:
			break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint8_t outcome){
	//Implement Predictor training
	switch (bpType) {
		case STATIC:
			return;
		case GSHARE:
			gshare_train(outcome);
		case TOURNAMENT:
			tournament_train(outcome);
		case CUSTOM:
			TAGE_train(pc,outcome);
		default:
			break;
	}
}


_Bool gshare_predict(uint32_t pc){
	uint32_t pcLowBits=pc&global_mask;
	GHR=GHR&global_mask;
	BHT_index=GHR^pcLowBits;
	if(ST==BHT[BHT_index]||WT==BHT[BHT_index])
		return TAKEN;
	else
		return NOTTAKEN;
}

void gshare_train(uint8_t outcome){
	GHR=GHR<<1;
	if(outcome==TAKEN){
		GHR=GHR|1;
		if(BHT[BHT_index]!=ST)
			BHT[BHT_index]+=1;
	}else{
		if(BHT[BHT_index]!=SN)
			BHT[BHT_index]-=1;
	}
	GHR=GHR&global_mask;
}

_Bool tournament_predict(uint32_t pc){

	//Local prediction
	local_PHT_index=pc&local_pc_mask;
	local_BHT_index=local_PHT[local_PHT_index]&local_PHT_mask;
	if(ST==local_BHT[local_BHT_index]||WT==local_BHT[local_BHT_index])
		local_res=TAKEN;
	else
		local_res=NOTTAKEN;
	
	//Global prediction
	GHR=GHR&global_mask;
	BHT_index=GHR;
	if(ST==BHT[BHT_index]||WT==BHT[BHT_index])
		global_res=TAKEN;
	else
		global_res=NOTTAKEN;
	
	//Selection
	if(SG==SHT[BHT_index]||WG==SHT[BHT_index])
		return global_res;
	else
		return local_res;
}

void tournament_train(uint8_t outcome){
	GHR=GHR<<1;
	local_PHT[local_PHT_index]=local_PHT[local_PHT_index]<<1;
	if(outcome==TAKEN){
		GHR=GHR|1;
		local_PHT[local_PHT_index]=local_PHT[local_PHT_index]|1;
		if(local_BHT[local_BHT_index]!=ST)
			local_BHT[local_BHT_index]+=1;
		if(BHT[BHT_index]!=ST)
			BHT[BHT_index]+=1;
	}else{
		if(local_BHT[local_BHT_index]!=SN)
			local_BHT[local_BHT_index]-=1;
		if(BHT[BHT_index]!=SN)
			BHT[BHT_index]-=1;
	}
	if(local_res==outcome&&global_res!=outcome){
		if(SHT[BHT_index]!=SL)
			SHT[BHT_index]+=1;
	}else if(global_res==outcome&&local_res!=outcome){
		if(SHT[BHT_index]!=SG)
			SHT[BHT_index]-=1;
	}
	GHR=GHR&global_mask;
	local_PHT[local_PHT_index]=local_PHT[local_PHT_index]&local_PHT_mask;

}

_Bool TAGE_predict(uint32_t pc){

	//Basic predictor makes prediction
	TAGE_BHT_index=pc&TAGE_BHT_mask;
	_Bool basic_res=(TAGE_BHT[TAGE_BHT_index]>=WT);
	firstChoice_table=-1;
	alter_table=-1; //alter_num, provider_num==-1 means it's from BHT of basic predictor, otherwise it's from partial tables
	firstChoice_res=basic_res;
	alter_res=basic_res;
	firstChoice_index=TAGE_BHT_index;
	alter_index=TAGE_BHT_index;

	//Search whether there is an available one in partial tables
	for(int i=0;i<PAR_TABLE_NUM;i++){
		uint32_t index=TAGE_index_hash(TAGE_GHR,pc,TAGE_GHR_mask[i],i);  //Compute hash index and hash tag
		uint32_t tag=TAGE_tag_hash(TAGE_GHR,pc,TAGE_GHR_mask[i],i);
		partial_table_lastIndice[i]=index;
		partial_table_lastTags[i]=tag; //Save the indice and tags to avoid recompute in train stage
		if(partial_table_TAG[i][index]==tag){
			alter_table=firstChoice_table;
			alter_index=firstChoice_index;
			alter_res=firstChoice_res; //Partial table hit, thus, previous first choice become alternative and current one become first choice. (Since TAGE always try to use the result from the longest partial table)
			firstChoice_table=i;
			firstChoice_index=index;
			firstChoice_res=(partial_table_BH[i][index]>=WT); 
		}
	}
	return firstChoice_res;
}

void TAGE_train(uint32_t pc,uint8_t outcome){

	//Update alternative's USE label
	if(alter_res!=firstChoice_res && alter_table!=-1){
		// Update behaviour in original paper 
		// if(firstChoice_res==outcome && partial_table_USE[alter_table][alter_index]!=S_US)
		// 	partial_table_USE[alter_table][alter_index]+=1;
		// if(firstChoice_res!=outcome && partial_table_USE[alter_table][alter_index]!=S_UN)
		// 	partial_table_USE[alter_table][alter_index]-=1;

		//But seems like this one has a little bit better performance
		if(firstChoice_res==outcome && partial_table_USE[alter_table][alter_index]!=S_UN)
			partial_table_USE[alter_table][alter_index]-=1;
		if(firstChoice_res!=outcome && partial_table_USE[alter_table][alter_index]!=S_US)
			partial_table_USE[alter_table][alter_index]+=1;
	}

	//Update firstChoice's BH
	if(outcome==TAKEN){
		if(-1==firstChoice_table){
			if(TAGE_BHT[firstChoice_index]!=ST)
				TAGE_BHT[firstChoice_index]+=1;
		}else{
			if(partial_table_BH[firstChoice_table][firstChoice_index]!=ST)
				partial_table_BH[firstChoice_table][firstChoice_index]+=1;
		}
	}else{
		if(-1==firstChoice_table){
			if(TAGE_BHT[firstChoice_index]!=SN)
				TAGE_BHT[firstChoice_index]-=1;
		}else{
			if(partial_table_BH[firstChoice_table][firstChoice_index]!=SN)
				partial_table_BH[firstChoice_table][firstChoice_index]-=1;
		}
	}

	//If prediction is wrong, update several things
	if(firstChoice_res!=outcome){
		//If firstChoice isn't from the longest partial table, allocate entry in partial table with longer length
		if(firstChoice_table!=PAR_TABLE_NUM-1){
			uint32_t avail_tables[PAR_TABLE_NUM];
			int avail_num=0;
			//Count how many partial tables have the available corresponding entry
			for(int i=0;i<PAR_TABLE_NUM;i++){
				if(i>firstChoice_table && partial_table_USE[i][partial_table_lastIndice[i]]==S_UN){
					avail_tables[i]=1;
					avail_num+=1;
				}else
					avail_tables[i]=0;
			}
			if(avail_num==0){  //If none is available, decrease the useful mark of corresponding entry in each partial table
				for(int i=firstChoice_table+1;i<PAR_TABLE_NUM;i++)
					partial_table_USE[i][partial_table_lastIndice[i]]-=1;
			}else{  //Randomly allocate an entry in those available partial tables
				int rand_chosen_table=rand_allocate(avail_tables,avail_num);
				//Set the tag
				partial_table_TAG[rand_chosen_table][partial_table_lastIndice[rand_chosen_table]]=partial_table_lastTags[rand_chosen_table];
				//Set the useful mark
				partial_table_USE[rand_chosen_table][partial_table_lastIndice[rand_chosen_table]]=S_US; 
				//Should set it as S_UN according to original paper, however, in practice S_US has the best performance on our dataset

				//Clean the old predict res
				if(outcome==TAKEN)
					partial_table_BH[rand_chosen_table][partial_table_lastIndice[rand_chosen_table]]=WT;
				else
					partial_table_BH[rand_chosen_table][partial_table_lastIndice[rand_chosen_table]]=WN;
			}
		}
	}

	//Update GHR at the last, since allocate step needs to use unchanged GHR
	TAGE_GHR=TAGE_GHR<<1;
	if(outcome==TAKEN)
		TAGE_GHR=TAGE_GHR|1;
	//TAGE_GHR=TAGE_GHR; Because GHR takes whole 64 bits, no need to do masking like other predictors.
}

uint32_t TAGE_index_hash(uint64_t GHR,uint32_t pc,uint64_t GHR_mask,uint32_t table_num){
	uint32_t hash_res=pc+(pc>>TAGE_PAR_INDEX_BITS[table_num]);
	uint32_t partial_GH= (uint32_t) GHR&GHR_mask;
	uint32_t par_index_mask=INTpow(2,TAGE_PAR_INDEX_BITS[table_num])-1;

	//Use fold hash function
	int len=(int)ceil(64/TAGE_PAR_INDEX_BITS[table_num]);
	for(int i=0;i<len;i++){
		hash_res+=partial_GH&par_index_mask;
		partial_GH=partial_GH>>TAGE_PAR_INDEX_BITS[table_num];
	}
	hash_res=hash_res&par_index_mask;
	
	return hash_res;
}

uint32_t TAGE_tag_hash(uint64_t GHR,uint32_t pc,uint64_t GHR_mask,uint32_t table_num){
	uint32_t hash_res=pc;
	uint32_t partial_GH= (uint32_t) GHR&GHR_mask;
	uint32_t par_index_mask=INTpow(2,TAGE_PAR_INDEX_BITS[table_num])-1;

	int len1=(int)ceil(64/TAGE_PAR_INDEX_BITS[table_num]);
	uint32_t tmp1=partial_GH;
	for(int i=0;i<len1;i++){
		hash_res+=tmp1&par_index_mask;
		tmp1=tmp1>>TAGE_PAR_INDEX_BITS[table_num];
	}

	//Use the second fold hash function to avoid conflict with hash index
	int len2=(int)ceil(64/(TAGE_PAR_INDEX_BITS[table_num]-1));
	uint32_t tmp2=partial_GH;
	for(int i=0;i<len2;i++){
		hash_res+=2*(tmp2&par_index_mask);
		tmp2=tmp2>>TAGE_PAR_INDEX_BITS[table_num];
	}

	hash_res=hash_res&par_index_mask;
	return hash_res;
}

uint32_t rand_allocate(uint32_t* avail_tables, int avail_num){
	//Generate a rand num, the probabilty of its least bit is 0 is 0.5
	//the probabilty of least two bits are 01 is 0.25 and so on 
	//xxx0 P=0.5    --> choose the first available partial table
	//xx01 P=0.25   --> choose the second available partial table
	//x011 P=0.125  --> ...
	//...

	uint32_t rand_res=(uint32_t) rand();
	int avail_table_index=0;
	//compute which table to choose according to rand_res
	while(avail_table_index<avail_num){
		if(((rand_res^0)&1)==0)  //((rand_res^0)&1)!=(rand_res^0)&1 ...Don't know why...
			break;
		if(avail_table_index==avail_num-1)
			break;
		rand_res=rand_res>>1;
		avail_table_index++;
	}
	//Get the table number of that one
	int cnt=0;
	for(int i=0;i<PAR_TABLE_NUM;i++){
		if(avail_tables[i]==1){
			if(cnt==avail_table_index)
				return i;
			cnt+=1;
		}
	}
	fprintf(stderr,"rand_allocate return error!");
	return PAR_TABLE_NUM-1;  //Should never return at this place, just in case
}

int INTpow(int base,int pow){
    int res=1;
    for(int i=0;i<pow;i++){
        res*=base;
    }
    return res;
}


