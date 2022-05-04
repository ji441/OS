#include <iostream>
#include <vector>
#include <stdio.h>
#include <deque>
#include <unistd.h>
#include <climits>
#include <string.h>
using namespace std;
const int maxpagenumber = 64;
const int maxframenumber = 128;
int framenum = 16;
int totalproc=0;
//freepool of frames
deque<int> freepool;
FILE* fp;
/*buffer for reading the file stream fp*/
char buf[256];
//various instruction counter
int instcounter = 0;
int ctx_switches = 0,process_exits = 0;
unsigned long long int cost = 0;
//option flags:
int Oflag =1,Sflag=1,Fflag=1,Pflag=1;
//define macro for tracing
#define traceO(fmt...)  do{if (Oflag == 1){ printf(fmt); fflush(stdout);}} while(0)
#define traceS(fmt...)  do{if (Sflag == 1){ printf(fmt); fflush(stdout);}} while(0)
#define traceF(fmt...)  do{if (Fflag == 1){ printf(fmt); fflush(stdout);}} while(0)
#define traceP(fmt...)  do{if (Pflag == 1){ printf(fmt); fflush(stdout);}} while(0)
/*data structure part*/
enum algmod {F,R,C,E,A,W};
struct pte_t{
    unsigned int present         :1;
    unsigned int referenced      :1;
    unsigned int modified        :1;
    unsigned int write_protect   :1;
    unsigned int pagedout        :1;
    unsigned int frame           :7;
    //record if we have checked this pte is in vma
    unsigned int check           :1;
    //if this pte is in vma of corresponding process
    unsigned int invmas          :1;
    unsigned int filemapped      :1;
    unsigned int                 :17;
};
struct vma
{
    int vstart =0;
    int vend = 0;
    bool wpprotect = 0;
    bool filemap = 0;
    void printvma()
    {
        printf("%d %d %d %d\n",vstart,vend,filemap,wpprotect);
    }
};
struct frame_entry{
    //reverse mapping, a pointer to the corresponding page table entry
    pte_t* vpage = nullptr;
    int procid;
    int pagenumber;
    //used for working set
    int tau;
    int time_last_used;
    unsigned int age    :32;
};
struct procstats{
    int unmaps;
    int maps;
    int ins;
    int outs;
    int fins;
    int fouts;
    int zeros;
    int segv;
    int segprot;
};
//frame table maintained gloabally
frame_entry frame_table[maxframenumber];
class process
{
public:
    pte_t page_table[maxpagenumber];
    vector<vma> vmas;
    int procid;
    procstats pstats;
    void printvmas()
    {
        for(int i=0;i<vmas.size();i++)
        {
            vmas[i].printvma();
        }
    }
    process()
    {
        totalproc++;
        procid=totalproc-1;
        //initialize the pte when process is created.
        for(int i=0;i<maxpagenumber;i++)
        {
            page_table[i]={0,0,0,0,0,0,0,0,0};
        }
        pstats={0,0,0,0,0,0,0,0,0};
    }
};
//algorithm class
class Pager
{
public:
    virtual int select_victim_frame() {return 0;}
    //used for aging method to reset age.
    virtual void resetAge(int findex) {}
    virtual void resetLastuse(int findex) {}
};
class FIFO : public Pager
{
public:
    int hand;
    FIFO()
    {
        hand=0;
    }
    virtual int select_victim_frame()
    {
        //select hand and hand advanced
        int res = hand;
        hand++;
        //wrap around if hand have gone through all frames
        if(hand == framenum) hand=0;
        return res;
    }
};
class CLOCK : public FIFO
{
public:
    int select_victim_frame()
    {
        while(frame_table[hand].vpage->referenced !=0)
        {
            frame_table[hand].vpage->referenced = 0;
            hand++;
            if(hand == framenum) hand=0;
        }
        int res = hand;
        hand++;
        if(hand == framenum) hand=0;
        return res;

    }
};
//this is the code from lab2 to get randomnumbers. used in class RANDOM
int randlimit;
int ofs = 0;
vector<int> randvals;
int myrandom(int burst)
{
    //now the burst is framenum here, the size of frame table.
    if(ofs == randlimit) ofs=0;
    return (randvals[ofs++] % burst);
}
class RANDOM : public Pager
{
public:
    int select_victim_frame()
    {
        return myrandom(framenum);
    }
};
class NRU : public Pager
{
public:
    int hand = 0;
    //timer to decide whether to reset R bits.
    int timer = 0;
    int select_victim_frame()
    {

        //find the lowest class
        int minclass = 4;
        int res;
        for(int i=hand;i< hand+framenum;i++)
        {
            //class value = 2*R+M
            int classval = 2*frame_table[i%framenum].vpage->referenced + frame_table[i%framenum].vpage->modified;
            if(classval < minclass)
            {
                minclass = classval;
                res = i % framenum;
            }
            //if we find 0 class we stop searching
            if(classval ==0) break;
        }
        hand = (res+1)%framenum;
        //if 50 or more instructions passed since last reset, we reset R bit and timer to current time.
        if((instcounter - timer) >= 50)
        {
            for(int i=0;i<framenum;i++)
            {
                if(frame_table[i].vpage !=nullptr)
                {
                    frame_table[i].vpage->referenced = 0;
                }
            }
            timer = instcounter;
        }
        return res;
    }
};
class Aging : public Pager
{
public:
    int hand = 0;
    int select_victim_frame()
    {
        //do the aging process for each active frame
        for(int i=0;i<framenum;i++)
        {
            if(frame_table[i].vpage != nullptr)
            {
                frame_table[i].age = frame_table[i].age >> 1;
                //if the referenced bit of the linked pte is 1, set it, else there is no need to do that.
                if(frame_table[i].vpage->referenced ==1)
                {
                    frame_table[i].age = (frame_table[i].age | 0x80000000);
                }
                //reset R bit.
                frame_table[i].vpage->referenced = 0;
            }
        }
        unsigned long long int minval = ULLONG_MAX;
        int res;
        //search from hand
        for(int i = hand;i< (hand+framenum);i++)
        {
            if(frame_table[i% framenum].age < minval)
            {
                minval = frame_table[i% framenum].age;
                res=i%framenum;
            }
        }
        hand= (res+1)%framenum;
        return res;

    }
    void resetAge(int findex)
    {
        frame_table[findex].age = 0;
    }
};
class WorkingSet : public Pager
{
public:
    int hand = 0;
    int select_victim_frame()
    {
        //record the mintime of lastuse time, initializing with instcounter +1;
        int mintime = instcounter+1;
        int res=hand;
        //scan frame table from hand
        for(int i= hand;i< framenum+hand;i++)
        {
            // when R bit is 1, update the bit and set last use time to current time
            if(frame_table[i%framenum].vpage->referenced == 1)
            {
                frame_table[i%framenum].time_last_used = instcounter;
                frame_table[i%framenum].vpage->referenced = 0;
            }
            //when R bit is not set, and t >= 50 we select it as victim
            else if(frame_table[i%framenum].vpage->referenced == 0 && (instcounter - frame_table[i%framenum].time_last_used) > 49 )
            {
                res= i % framenum;
                break;
            }
            else
            {
                if(frame_table[i%framenum].time_last_used < mintime)
                {
                    mintime = frame_table[i%framenum].time_last_used;
                    res = i%framenum;
                }
            }
        }
        hand = (res+1)%framenum;
        return res;
    }
    void resetLastuse(int findex)
    {
        frame_table[findex].time_last_used = instcounter;
    }
};
/*function part*/
bool get_next_instruction(char* operation,int* vpage)
{
    /*get to the end of file, the false is returned.*/
    if(fgets(buf,256,fp) == NULL) return false;
    else
    {
        /*if the line start with '#' we go to nextline*/
        if(buf[0]=='#')
        {
            return get_next_instruction(operation,vpage);
        }
        sscanf(buf,"%c %d",operation,vpage);
        instcounter++;
        return true;
    }
}
Pager* the_pager;
int get_frame()
{
    int frameindex;
    //if there is no more free frame, call pager to select victim frame
    if(freepool.empty()) frameindex = the_pager->select_victim_frame();
    //else we just get it from front of free queue.
    else
    {
        frameindex = freepool.front();
        freepool.pop_front();
    }
    return frameindex;
}
//simulation part

void simulation(vector<process*> Allproc,algmod algorithm)
{
    if(algorithm == F) the_pager = new FIFO();
    else if(algorithm == C) the_pager = new CLOCK();
    else if(algorithm == R) the_pager = new RANDOM();
    else if(algorithm == E) the_pager = new NRU();
    else if(algorithm == A) the_pager = new Aging();
    else if(algorithm == W) the_pager = new WorkingSet();
    //create variable to store values of instruction
    char operation;
    int vpage;
    process* current_process;
    while(get_next_instruction(&operation,&vpage))
    {
        //print the instruction id and instruction
        traceO("%d: ==> %c %d\n",instcounter-1,operation,vpage);
        //context switch, set the current process to corresponding one, then break loop to get next instruction.
        if(operation == 'c')
        {
            current_process = Allproc[vpage];
            ctx_switches++;
            //each c instruction cost 130;
            cost+=130;
            continue;
        }
        //process exit.
        else if(operation == 'e')
        {
            traceO("EXIT current process %d\n",current_process->procid);
            process_exits++;
            //each e instruction cost 1250;
            cost+=1250;
            //traverse current process's page table
            for(int i =0;i<maxpagenumber;i++)
            {
                //if the pte is active we need deal with unmap and FOUT
                if(current_process->page_table[i].present == 1)
                {
                    traceO(" UNMAP %d:%d\n",current_process->procid,i);
                    //unmap cost 400
                    cost+=400;
                    current_process->pstats.unmaps++;
                    //do FOUT.
                    if(current_process->page_table[i].filemapped && current_process->page_table[i].modified)
                    {
                        traceO(" FOUT\n");
                        //fout cost 2400
                        cost+=2400;
                        current_process->pstats.fouts++;
                    }
                    //free those frames,
                    int frameindex = current_process->page_table[i].frame;
                    freepool.push_back(frameindex);
                    frame_table[frameindex].vpage = nullptr;

                }
                //reset bits for referenced modified and present
                current_process->page_table[i].present = 0;
                current_process->page_table[i].referenced = 0;
                current_process->page_table[i].modified = 0;
                current_process->page_table[i].pagedout = 0;
            }

            continue;
        }
        //then real instruction:
        //r/w cost 1;
        cost+=1;
        pte_t* pte = &(current_process->page_table[vpage]);
        if( pte->present == 0)
        {
            //first check if this pte is in vmas of the corresponding process
            if(pte->check == 0)
            {
                //if we have not checked this page before, we do a iterative search in vmas of this process.
                for(int i=0;i< (current_process->vmas).size();i++)
                {
                    int vmasize = (current_process->vmas).size();
                    vma tempvma = current_process->vmas[i];
                    if(vpage >= tempvma.vstart && vpage <=tempvma.vend)
                    {
                        //if we find a map in vmas, then set the invmas and check to true, and stop searching.
                        pte->check = 1;
                        pte->invmas =1;
                        //inherit property (filemapped and write protection) from VMA !!
                        pte->filemapped = tempvma.filemap;
                        pte->write_protect = tempvma.wpprotect;
                        break;
                    }
                }
                //after iterative search, meaning we do not find any match, so it is not in vmas,we set the checkbit.
                pte->check = 1;
            }
            //after iterative check, if the pte is not in vmas of the process, print error and move on to next inst
            if(pte->invmas == 0)
            {
                traceO(" SEGV\n");
                //segv cost 340, update cost and process status
                cost+=340;
                current_process->pstats.segv++;
                continue;
            }
            //now we are sure that it is in vmas.
            int frameindex = get_frame();
            frame_entry *currentframe = &frame_table[frameindex];
            //if the old frame page was mapped: deal with unmap first

            if(currentframe->vpage != nullptr)
            {
                pte_t* previouspte= currentframe->vpage;
                //print unmap first
                traceO(" UNMAP %d:%d\n",currentframe->procid,currentframe->pagenumber);
                //unmap cost 400
                cost+=400;
                Allproc[currentframe->procid]->pstats.unmaps++;
                //check for OUT/FOUT
                if(previouspte->filemapped && previouspte->modified)
                {
                    traceO(" FOUT\n");
                    //fout cost 2400
                    cost+=2400;
                    Allproc[currentframe->procid]->pstats.fouts++;
                }
                //in the case of paged out, change the pagedout bit of that pte.
                else if(previouspte->modified)
                {
                    previouspte->pagedout = 1;
                    traceO(" OUT\n");
                    //out cost 2700
                    cost+=2700;
                    Allproc[currentframe->procid]->pstats.outs++;
                }
                //when previous bit is kicked out we reset some bit except pagedout
                previouspte->present = 0;
                previouspte->referenced = 0;
                previouspte->modified = 0;
            }
            //now deal with map part
            pte->frame = frameindex;
            //finally the pte is now present.
            pte->present = 1;
            currentframe->vpage = pte;
            currentframe->procid = current_process->procid;
            currentframe->pagenumber = vpage;
            //if the mapped pte is FIN/IN/ZERO
            if(pte->filemapped)
            {
                traceO(" FIN\n");
                //fin cost 2800
                cost+=2800;
                current_process->pstats.fins++;
            }
            else if(pte->pagedout)
            {
                traceO(" IN\n");
                //in cost 3100
                cost+=3100;
                current_process->pstats.ins++;
            }
            else
            {
                traceO(" ZERO\n");
                //zero cost 140
                cost+=140;
                current_process->pstats.zeros++;
            }
            traceO(" MAP %d\n",frameindex);
            //map cost 300
            cost+=300;
            current_process->pstats.maps++;
            the_pager->resetAge(frameindex);
            the_pager->resetLastuse(frameindex);
        }
        //check write protection
        if(operation =='w')
        {
            if(pte->write_protect == 1)
            {
                traceO(" SEGPROT\n");
                //segport cost 420
                cost+=420;
                current_process->pstats.segprot++;
            }
            else pte->modified = 1;
        }
        //set reference bit
        pte->referenced = 1;

    }
}
int main(int argc, char* argv[])
{
    int GETIN;
    char algomode;
    char optioninput[5];
    char* infname;
    char* rfname;
    algmod algorithm;
    while((GETIN=getopt(argc,argv,"f:a:o:")) != -1)
    {
        switch(GETIN)
        {
        case 'f':
            if((sscanf(optarg,"%d",&framenum)) <1)
            {
                printf("number after f\n");
                exit(1);
            }
            break;
        case 'a':
            if((sscanf(optarg,"%c",&algomode)) <1)
            {
                printf("char after a\n");
                exit(1);
            }
            break;
        case 'o':
            if((sscanf(optarg,"%s",optioninput)) <1)
            {
                printf("option after o\n");
                exit(1);
            }
            break;

        }
    }
    if((argc - optind) < 2)
    {
        printf("missing file name\n");
        exit(1);
    }
    infname=argv[optind];
    rfname=argv[optind+1];
    //get input from rfile
    FILE* rf = fopen(rfname,"r");
    char randbuf[256];
    fgets(randbuf,256,rf);
    randlimit = atoi(strtok(randbuf," \n"));
    /*get all randvals from the rfile*/
    while(fgets(randbuf,256,rf) != NULL){
        char* tok=strtok(randbuf," \n");
        int randnum= atoi(tok);
        randvals.push_back(randnum);
    }
    //set algorithm well
    if(algomode == 'f') algorithm = F;
    else if (algomode == 'r') algorithm = R;
    else if (algomode == 'c') algorithm = C;
    else if (algomode == 'e') algorithm = E;
    else if (algomode == 'a') algorithm = A;
    else if (algomode == 'w') algorithm = W;
    //set flag for options.
    for(int i=0;i< sizeof(optioninput)/sizeof(char);i++)
    {
        if (optioninput[i]=='O') Oflag =1;
        if (optioninput[i]=='P') Pflag =1;
        if (optioninput[i]=='S') Sflag =1;
        if (optioninput[i]=='F') Fflag =1;
    }

    /*record all process*/
    vector<process*> Allproc;
    fp=fopen(infname,"r");
    /*get total number of process first*/
    int total_process_number;
    while(fgets(buf,256,fp) != NULL)
    {
        /*once get the number stop loop.*/
        if(buf[0] != '#')
        {
            sscanf(buf,"%d",&total_process_number);
            break;
        }
    }
    for(int i = 0;i<total_process_number;i++)
    {
        process* temp = new process();
        /*get total number of vmas for each process.*/
        int total_vmasnumber;
        while(fgets(buf,256,fp) != NULL)
        {
            if(buf[0] != '#')
            {
                sscanf(buf,"%d",&total_vmasnumber);
                break;
            }
        }
        /*push in the vmas for current process*/
        for(int j=0;j<total_vmasnumber;j++)
        {
            int a,b,c,d;
            while(fgets(buf,256,fp) != NULL)
            {
                if(buf[0] != '#')
                {
                    sscanf(buf,"%d %d %d %d",&a,&b,&c,&d);
                    temp->vmas.push_back({a,b,c,d});
                    break;
                }
            }
        }
        Allproc.push_back(temp);

    }
    //initialize free pool
    for(int i=0;i<framenum;i++) freepool.push_back(i);
    simulation(Allproc,algorithm);
    //P option,pagetable output, go through all processes' page table
    for(int i =0;i<Allproc.size();i++)
    {
        traceP("PT[%d]:",i);
        //go through each page, i.e. pte in the corresponding page table
        for(int j=0;j<maxpagenumber;j++)
        {
            if(Allproc[i]->page_table[j].present == 1)
            {
                char rbit = '-',mbit='-',pbit='-';
                if(Allproc[i]->page_table[j].referenced ==1) rbit='R';
                if(Allproc[i]->page_table[j].modified ==1) mbit='M';
                if(Allproc[i]->page_table[j].pagedout ==1) pbit='S';
                //if it is valid, we print
                traceP(" %d:%c%c%c",j,rbit,mbit,pbit);
            }
            else
            {
                if(Allproc[i]->page_table[j].pagedout == 1) traceP(" #");
                else traceP(" *");
            }
        }
        traceP("\n");
    }
    //F option output
    traceF("FT:");
    for(int i=0;i<framenum;i++)
    {
        if(frame_table[i].vpage != nullptr)
        {
            traceF(" %d:%d",frame_table[i].procid,frame_table[i].pagenumber);
        }
        else traceF(" *");
    }
    traceF("\n");
    //S output
    //per process summary
    for(int i=0;i<Allproc.size();i++)
    {
        traceS("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",i,Allproc[i]->pstats.unmaps,Allproc[i]->pstats.maps,Allproc[i]->pstats.ins,Allproc[i]->pstats.outs,
               Allproc[i]->pstats.fins,Allproc[i]->pstats.fouts,Allproc[i]->pstats.zeros,Allproc[i]->pstats.segv,Allproc[i]->pstats.segprot);
    }
    traceS("TOTALCOST %lu %lu %lu %llu %lu\n",instcounter,ctx_switches,process_exits,cost,sizeof(pte_t));


}
