//ECE 4/585 Final Project
//Group 2 - Erik Fox, Tapasya Bhatnagar, Alec Wiese, Ryan Writz


//example to run code in debug: ./a.out Exampletoparse.txt 1
//exampe to run without debug: ./a.out Exampletoparse.txt


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define QUEUELEN 16
#define MEMREQLEN 15

#define CMD_WRITE "WRITE"
#define CMD_READ  "READ"
#define CMD_FETCH "FETCH"

//#define MASK_ROW		0x00000001FFFE0000
//#define MASK_BANK		0x0000000000018000
//#define MASK_COL_UPPER	0x0000000000007F00
//#define MASK_BANK_GROUP       0x00000000000000C0
//#define MASK_COL_LOWER	0x0000000000000038

//#define debugQueue 1

#define OPEN_PAGE_DURATION 150 //DRAM CYCLES

#define tRCD	24	//Time before read issued after activate
#define tRAS	52	//time before Precharge issued after activate
#define tRP		24	//Duration of precharge
#define tRC		76	//(tRAS + tRP)	//Duration of row read/write cycle

#define tRTP	12	//Time between a read and a precharge command

#define tCCD_L	8	//Successive read/write commands within a bank group
#define tCCD_S	4	//Successive read/write commands between bank groups

#define tWR		20	//interval between write data and precharge command

#define tBURST	4	//Burst length
#define tWTR_L	12	//end of write data burst to column read command delay within a bank group
#define tWTR_S	4	//end of write data burst to column read command delay between bank groups

#define waitBurstLong (tWTR_L + tBURST)
#define waitBurstShort (tWTR_S + tBURST)


#define tRRD_L	6	//Activate to activate command within a bank group
#define tRRD_S	4	//Activate to activate command between bank groups

#define R 16 //Row Bits
#define B 2  //Bank Bits
#define UC 7  //Upper Column Bits
#define BG 2  //Bank Group Bits
#define LC 3  //Lower Column Bits
#define BS 3	//Byte select bits

#define TOTALBANKS 16//( (B^2) * (BG^2) )

#define MASK_ROW (((1LL << R) - 1) <<(B+UC+BG+LC+BS)) //MASK_ROW = Left Shift (2^R-1) by (B+UC+BG+LC+3) bits 
#define MASK_BANK (((1LL << B) - 1) <<(UC+BG+LC+BS))     //MASK_BANK = Left Shift (2^B-1) by (UC+BG+LC+3) bits 
#define MASK_COL_UPPER (((1LL << UC) - 1) <<(BG+LC+BS))  //MASK_COL_UPPER = Left Shift (2^UC-1) by (BG+LC+3) bits 
#define MASK_BANK_GROUP (((1LL << BG) - 1) <<(LC+BS))  //MASK_BANK_GROUP = Left Shift (2^BG-1) by (LC+3) bits
#define MASK_COL_LOWER (((1LL << LC) - 1) <<(BS))  //MASK_COL_LOWER = Left Shift (2^LC-1) by 3 bits
#define ull unsigned long long

int queueOccupied = 0;

unsigned long long count = 0, countLast = -2, SDRAMcycle = -1, SDRAMcycleLast = -1;
unsigned row, bank, col_upper, bank_group, col_lower, column;

FILE *outputFile;

void bitSlice(ull address)
{
	row			= (address & MASK_ROW) >> (B + UC + BG + LC + BS);		//Shift all the way to the right
	bank		= (address & MASK_BANK) >> (UC + BG + LC + BS);		//Shift all the way to the right
	col_upper	= (address & MASK_COL_UPPER) >> (BG + LC + BS);	//Shift to upper position of column (where bank group currently is)
	bank_group	= (address & MASK_BANK_GROUP) >> (LC + BS);	//Shift all the way to the right
	col_lower	= (address & MASK_COL_LOWER) >> BS;	//Shift all the way to the right

	column = (col_upper << BS) | col_lower;	//gives us our full column address
	return;
}
struct cpu_req
{
	char instruct[MEMREQLEN];
	unsigned long long time;
	unsigned long long hex;
	int activate, read, precharge, cycles;
	int next;
	int prev;
};

struct bankOpenRow
{
	int openRow;
	ull openRowDuration;
	ull counter_tRP;
};
struct bankOpenRow bankOpenRows[TOTALBANKS];

//Command counters
ull counter_tCCD_L = 0, counter_tCCD_S = 0, counter_tRTP = 0, counter_tWR = 0, counter_tRCD = 0, counter_tRAS = 0,
	counter_waitBurstLong = 0, counter_waitBurstShort = 0, counter_tRRD_L = 0, counter_tRRD_S = 0;
//
int lastBankAccessed = -1, currentBank = -1, lastBankGroup = -1, lastBankGroupActivated = -1;

int build_freelist(int * stack)
{
	for(int i = 0; i< QUEUELEN; ++i)
	{
		stack[i]=i;
	}			
	return 1;
}

int pop_freelist( int * head, int * stack)
{
	if(*head==QUEUELEN)
	{
		return -1;
	}
	else
	{
		++(*head);
	}
	
	return stack[(*head)-1];
}

int push_freelist(int * stack, int * head, int  tail, int val)
{
	if((*head)-1==0)//Last available space in the freelist
	{	
		
		--(*head);
		stack[*head]=val;
		return -1;
	}
	else
	{
		if((*head)==QUEUELEN)//First item in freelist
		{			
			*head=tail;
			stack[*head]=val;		
		
			return 0;
		}
		else //in middle of queue
		{
			--(*head);
			stack[*head]=val;
		}
	}
	return 1;
}

int enqueue(struct cpu_req * queue, struct cpu_req mem,int* stack, int * freelist_head, int * queue_tail, int * queue_head)
{	
	queueOccupied = 1;
	int location_freelist;
	location_freelist=pop_freelist(freelist_head,stack);
	if(location_freelist==-1)
	{
		//	Queue Full
		return 0;
	}
	else if ((*freelist_head)-1==0)
	{
		//One thing left in the queue
		*queue_tail=location_freelist;
		*queue_head=location_freelist;
		queue[location_freelist]=mem;
		queue[location_freelist].next=location_freelist;
		queue[location_freelist].prev=location_freelist;

		queue[location_freelist].precharge = 0;
		queue[location_freelist].activate = 0;
		queue[location_freelist].read = 0;
		queue[location_freelist].cycles = 0;

		return 1;	
	}
	else
	{
		//Somewhere in the middle
		queue[(*queue_tail)].prev =location_freelist;
		queue[location_freelist]=mem;
		queue[location_freelist].prev=location_freelist;	
		queue[location_freelist].next=*queue_tail;
		*queue_tail= location_freelist;

		queue[location_freelist].precharge = 0;
		queue[location_freelist].activate = 0;
		queue[location_freelist].read = 0;
		queue[location_freelist].cycles = 0;
		return 1;
	}
}

int deq=0;
int dequeue(int location,int*stack, int* freelist_head, int freelist_tail, int* queue_head, int *queue_tail, struct cpu_req * queue)
{	
	int queue_status;
	queue_status=push_freelist(stack,freelist_head, freelist_tail, location);
	if((queue_status==-1))
	{ 
		*queue_head=queue[*queue_head].prev;
		queue[*queue_head].next=*queue_head;
		queueOccupied = 0;
		return 0;
		#ifdef debugQueue
			printf("Queue Empty\n");
			printf("queue_head : %d\n", *queue_head);
		#endif
	}
	else	
	{
		queueOccupied = 1;
		if(*freelist_head==0)
		{
			*queue_head=-1;
			*queue_tail=-1;
		}
		else
		{
			#ifdef debugQueue
				printf("location: %d, queue_head location: %d\n", location, *queue_head);
			#endif
			if(location==*queue_head)
			{
				*queue_head=queue[*queue_head].prev;
				queue[*queue_head].next=*queue_head;
				#ifdef debugQueue
					printf("dequeue @ head\n");
					printf("queue_head : %d\n", *queue_head);
				#endif
			}
			else if (location==*queue_tail)
			{
				#ifdef debugQueue
					printf("dequeue @ tail\n");
				#endif
				*queue_tail=queue[*queue_tail].next;
				queue[*queue_tail].prev=*queue_tail;
			}
			else
			{
				#ifdef debugQueue
					printf("dequeue in middle\n");
				#endif
				queue[queue[location].prev].next=queue[location].next;
				queue[queue[location].next].prev=queue[location].prev;

			}
		}
	}
	
	return 1;
}


int memoryController(int*stack, int* freelist_head, int freelist_tail, int* queue_head, int *queue_tail, struct cpu_req * queue, FILE *out_file)
{
	bitSlice(queue[*queue_head].hex);
	//Determine current bank group-bank combo
	currentBank = (bank_group << BG) | bank;

	int prechargeReq = 0, activateReq = 0, commandReq = 0;
	int SDRAMcountDifference = (count/2) - (countLast/2);
	countLast = count;
	//Increment open page durations
	int i;
	for (i = 0; i < TOTALBANKS; i++)
	{
		bankOpenRows[i].openRowDuration += SDRAMcountDifference;

		if (bankOpenRows[i].counter_tRP >= SDRAMcountDifference)
			bankOpenRows[i].counter_tRP -= SDRAMcountDifference;
		else
			bankOpenRows[i].counter_tRP = 0;
	}

	if (counter_tCCD_L >= SDRAMcountDifference)
		counter_tCCD_L -= SDRAMcountDifference;
	else
		counter_tCCD_L = 0;

	if (counter_tCCD_S >= SDRAMcountDifference)
		counter_tCCD_S -= SDRAMcountDifference;
	else
		counter_tCCD_S = 0;

	if (counter_tRTP >= SDRAMcountDifference)
		counter_tRTP -= SDRAMcountDifference;
	else
		counter_tRTP = 0;

	if (counter_tWR >= SDRAMcountDifference)
		counter_tWR -= SDRAMcountDifference;
	else
		counter_tWR = 0;

	if (counter_tRCD >= SDRAMcountDifference)
		counter_tRCD -= SDRAMcountDifference;
	else
		counter_tRCD = 0;

	if (counter_tRAS >= SDRAMcountDifference)
		counter_tRAS -= SDRAMcountDifference;
	else
		counter_tRAS = 0;

	if (counter_waitBurstLong >= SDRAMcountDifference)
		counter_waitBurstLong -= SDRAMcountDifference;
	else
		counter_waitBurstLong = 0;

	if (counter_waitBurstShort >= SDRAMcountDifference)
		counter_waitBurstShort -= SDRAMcountDifference;
	else
		counter_waitBurstShort = 0;

	if (counter_tRRD_L  >= SDRAMcountDifference)
		counter_tRRD_L  -= SDRAMcountDifference;
	else
		counter_tRRD_L  = 0;

	if (counter_tRRD_S  >= SDRAMcountDifference)
		counter_tRRD_S  -= SDRAMcountDifference;
	else
		counter_tRRD_S  = 0;

	//If the current request has not been evaluated yet
	if (queue[*queue_head].precharge == 0)
	{
		if ( (bankOpenRows[currentBank].openRow == row) //current row is precharged and activated...
			& (bankOpenRows[currentBank].openRowDuration <= OPEN_PAGE_DURATION) )
		{
			//PAGE HIT
			commandReq = 1;
			queue[*queue_head].precharge = 1;
			queue[*queue_head].activate = 1;
		}
		else
		{
			//PAGE MISS
			prechargeReq = 1;
		}
	}
	//Otherwise request was a page miss and has not done activate command yet
	else if (queue[*queue_head].activate == 0)
	{
		activateReq = 1;
	}
	//Otherwise request was a page miss with activate done, and no read/write command yet
	else if (queue[*queue_head].read == 0)
	{
		commandReq = 1;
	}

	/* REMINDER OF TIMING PARAMETER DEFINITIONS
		#define tRCD	24	//Time before read issued after activate
		#define tRAS	52	//time before Precharge issued after activate
		#define tRP		24	//Duration of precharge
		#define tRC		76	//(tRAS + tRP)	//Duration of row read/write cycle

		#define tRTP	12	//Time between a read and a precharge command

		#define tCCD_L	8	//Successive read/write commands within a bank group
		#define tCCD_S	4	//Successive read/write commands between bank groups

		#define tWR		20	//interval between write data and precharge command

		#define tBURST	4	//Burst length
		#define tWTR_L	12	//end of write data burst to column read command delay within a bank group
		#define tWTR_S	4	//end of write data burst to column read command delay between bank groups

		#define waitBurstLong (tWTR_L + tBURST)
		#define waitBurstShort (tWTR_S + tBURST)


		#define tRRD_L	6	//Activate to activate command within a bank group
		#define tRRD_S	4	//Activate to activate command between bank groups

	*/

	//activate, read, cycles, evaluated
	if (prechargeReq == 1)
	{
		//Timing involves... tRTP, tWR, tRAS, and tRP from the last precharge
		if ( (counter_tRTP == 0) & (counter_tWR == 0) & (counter_tRAS == 0) & (bankOpenRows[currentBank].counter_tRP == 0) )
		{
			queue[*queue_head].precharge = 1;
			fprintf(out_file, "%llu PRE %x %x\n", count, bank_group, bank); 
			bankOpenRows[currentBank].counter_tRP = tRP;
			
		}
		
	}
	else if (activateReq == 1)
	{
		ull counter_real_tRRD;
		if (lastBankGroup == bank_group)
		{
			counter_real_tRRD = counter_tRRD_L;
		}
		else
		{
			counter_real_tRRD = counter_tRRD_S;
		}
		if ( (bankOpenRows[currentBank].counter_tRP == 0) & (counter_real_tRRD == 0) )
		{
			bankOpenRows[currentBank].openRow = row;
			bankOpenRows[currentBank].openRowDuration = 0;

			fprintf(out_file, "%llu ACT %x %x %x\n", count, bank_group, bank, row); 

			queue[*queue_head].precharge = 1;
			queue[*queue_head].activate = 1;

			counter_tRAS = tRAS;
			counter_tRCD = tRCD;

			
			lastBankGroupActivated = currentBank;//only when activate occurs
		}
	}
	else if (commandReq == 1)
	{
		//printf("Command\n");
		ull counter_real_tCCD, counter_real_tWTR;
		if (lastBankGroup == bank_group)
		{
			counter_real_tCCD = counter_tCCD_L;
			counter_real_tWTR = counter_waitBurstLong;
		}
		else
		{
			counter_real_tCCD = counter_tCCD_S;
			counter_real_tWTR = counter_waitBurstShort;
		}

		//Read Timing involves...	tRCD, tCCD_L and tCCD_S
		//Write Timing involves...	tRCD, tCCD_L and tCCD_S
		if ( (counter_real_tCCD == 0) & (counter_tRCD == 0) )
		{
			int access = 0;
			if ( ( strcmp("READ", queue[*queue_head].instruct) == 0 ) | (strcmp("FETCH", queue[*queue_head].instruct) == 0) )
			{
				if (counter_real_tWTR == 0)
				{
					fprintf(out_file, "%llu RD %x %x %x\n", count, bank_group, bank, column);
					counter_tRTP = tRTP;
					access = 1;
				}
			}
			else if (strcmp("WRITE", queue[*queue_head].instruct) == 0)
			{
				fprintf(out_file, "%llu WR %x %x %x\n", count, bank_group, bank, column);
				counter_tWR = tWR;
				counter_waitBurstLong = waitBurstLong;
				counter_waitBurstShort = waitBurstShort;
				access = 1;
			}
			else
			{
				printf("Invalid Command for Address %llx\n", queue[*queue_head].hex);
				fprintf(out_file, "%llu ERROR %x %x %x\n", count, bank_group, bank, column);
			}
			if (access == 1)
			{
				lastBankAccessed = currentBank;//only when access occurrs
				lastBankGroup = bank_group;

				counter_tCCD_L = tCCD_L;
				counter_tCCD_S = tCCD_S;

				//Clean up queue entry before releasing it
				queue[*queue_head].precharge = 0;
				queue[*queue_head].activate = 0;
				queue[*queue_head].read = 0;

				#ifdef debugQueue
					printf("dequeue - cycles %d\n", queue[*queue_head].cycles);
				#endif
				++deq;		
				return dequeue(*queue_head, stack, freelist_head, freelist_tail, queue_head, queue_tail, queue);
			}
		}
		
	}

	++queue[*queue_head].cycles;

	return 1;
}



int main(int argc, char*argv[])
{
	int stack[QUEUELEN];
	int freelist_head=0;
	int freelist_tail=QUEUELEN-1;
	build_freelist(stack);

	struct cpu_req mem;
	struct cpu_req queue[QUEUELEN];
	int queue_head=0;
	int queue_tail=0;	
	FILE *fp;
	FILE * out_file;
	time_t t;
	int enq=1;
	int deqenq=0;
	int flag=0;
	fp = fopen(argv[1],"r");
	int i;


	//initialize open bank information
	for (i = 0; i < TOTALBANKS; i++)
	{
		bankOpenRows[i].openRow = -1;
		bankOpenRows[i].openRowDuration = -1;
		bankOpenRows[i].counter_tRP = 0;
	}


	if (fp==NULL)
	{
		printf("open error\n");
		exit(1);
	}
	char outputFileName[100] = "";

	strcat(outputFileName, "DRAMoutput-");
	strcat(outputFileName, argv[1]);

	out_file=fopen(outputFileName,"w");	

	if (out_file==NULL)
	{
		printf("open error\n");
		exit(1);
	}



	while(fscanf(fp, "%llu %s %llX",&mem.time, mem.instruct, &mem.hex)!=EOF)
	{
		mem.precharge = 0;
		mem.activate = 0;
		mem.read = 0;
		mem.cycles = 0;

		do{
			if(count<mem.time)//If our CPU count has not reached next memory request from file
			{
					while( (count<mem.time) )
					{ 
						if (queueOccupied == 1)
						{
							if(!(count%2))//Increment the SDRAMcycle if we are on an even clock cycle
							{
								memoryController(stack, &freelist_head, freelist_tail, &queue_head, &queue_tail, queue,out_file);
						
							}	
							++count;
						}
						else
						{
							count = mem.time;

						}
						
					}
					flag=enqueue(queue, mem, stack, &freelist_head, &queue_tail, &queue_head);// Zero if nothing enqueued
				
			}
			else if(flag==0)
			{
				flag=enqueue(queue, mem, stack, &freelist_head, &queue_tail, &queue_head);// Zero if nothing enqueued
				if(!(count%2))//Increment the SDRAMcycle if we are on an even clock cycle
				{
					memoryController(stack, &freelist_head, freelist_tail, &queue_head, &queue_tail, queue,out_file);
				}	
				++count;
			}
		
		}while(flag==0);	//The only way to exit is if the enqueue operation is successful		
		flag=0;
	}
		flag = 1;
		do{
				if(!(count%2))//Increment the SDRAMcycle if we are on an even clock cycle
				{
					flag=memoryController(stack, &freelist_head, freelist_tail, &queue_head, &queue_tail, queue,out_file);
				}	
				++count;
		}while( flag == 1);				

	return 1;
}
