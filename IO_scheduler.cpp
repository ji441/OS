#include <iostream>
#include <vector>
#include <stdio.h>
#include <deque>
#include <unistd.h>
#include <climits>
#include <string.h>
#include <cstdlib>
using namespace std;
int current_time=0;
int totalreq=0;
int tot_movement=0;

enum schedmode {I,J,S,C,F};
class IOrequest
{
public:
    int arrivetime;
    int address;
    int reqid;
    int starttime,endtime;
    IOrequest(int art,int ad)
    {
        arrivetime = art;
        address = ad;
        reqid = totalreq;
        totalreq++;
    }
    void printall()
    {
        printf("%5d: %5d %5d %5d\n",reqid,arrivetime,starttime,endtime);
    }
};

class IOsched
{
public:
    deque<IOrequest*> IOque;
    //the IOque serve as the activeq
    deque<IOrequest*> addq;
    virtual IOrequest* getIO(int head,int direction) {}
    virtual void addIO(IOrequest* tempIO)
    {
        IOque.push_back(tempIO);
    }
};

class FIFO : public IOsched
{
public:
    IOrequest* getIO(int head, int direction)
    {
        if(IOque.empty()) return nullptr;
        else
        {
            IOrequest* temp = IOque.front();
            IOque.pop_front();
            return temp;
        }
    }
};
class SSTF : public IOsched
{
public:
    IOrequest* getIO(int head, int direction)
    {
        if(IOque.empty()) return nullptr;
        else
        {
            int min_dist = INT_MAX;
            IOrequest* res;
            deque<IOrequest*>::iterator it_delete;
            for(auto it = IOque.begin();it != IOque.end();++it)
            {
                if( abs((*it)->address - head)< min_dist )
                {
                    min_dist= abs((*it)->address - head);
                    res = *it;
                    it_delete = it;
                }
            }
            IOque.erase(it_delete);
            return res;
        }
    }
};
class LOOK : public IOsched
{
public:
    IOrequest* getIO(int head, int direction)
    {
        if(IOque.empty()) return nullptr;
        else
        {
            int min_dist = INT_MAX;
            IOrequest* res = nullptr;
            deque<IOrequest*>::iterator it_delete;
            for(auto it = IOque.begin();it != IOque.end();++it)
            {
                int determinant = direction*((*it)->address - head);
                if(determinant >= 0 && determinant < min_dist)
                {
                    min_dist = determinant;
                    res = *it;
                    it_delete = it;
                }
            }
            if(res == nullptr) return getIO(head,-1*direction);
            IOque.erase(it_delete);
            return res;
        }
    }
};
class CLOOK : public IOsched
{
public:
    IOrequest* getIO(int head, int direction)
    {
        if(IOque.empty()) return nullptr;
        else
        {
            int min_dist = INT_MAX;
            int min_address = INT_MAX;
            IOrequest* res = nullptr;
            deque<IOrequest*>::iterator it_closet, it_min;
            for(auto it= IOque.begin();it != IOque.end();++it)
            {
                //in CLOOK we always try to search in the positive direction
                int determinant = (*it)->address - head;
                if(determinant >= 0 && determinant < min_dist)
                {
                    min_dist = determinant;
                    res = *it;
                    it_closet = it;
                }
                if((*it)->address < min_address)
                {
                    min_address = (*it)->address;
                    it_min = it;
                }
            }
            if(res == nullptr)
            {
                //if we do not find anymore in positive direction, we pick the IO with lowest address
                res = *it_min;
                IOque.erase(it_min);
            }
            else IOque.erase(it_closet);
            return res;

        }
    }
};
class FLOOK : public IOsched
{
public:
    deque<IOrequest*>* pactive = &IOque;
    deque<IOrequest*>* padd = &addq;
    //add is performed on addq
    void addIO(IOrequest* tempIO)
    {
        (*padd).push_back(tempIO);
    }
    IOrequest* getIO(int head, int direction)
    {
        //if active queue is run out we switch queue and try again.
        if((*pactive).empty())
        {
            deque<IOrequest*>* ptemp = pactive;
            pactive = padd;
            padd = ptemp;
            //if the addq is also empty we return nullptr
            if((*pactive).empty()) return nullptr;


        }
        //select only from activeq.

            int min_dist = INT_MAX;
            IOrequest* res = nullptr;
            deque<IOrequest*>::iterator it_delete;
            for(auto it = (*pactive).begin();it != (*pactive).end();++it)
            {
                int determinant = direction*((*it)->address - head);
                if(determinant >= 0 && determinant < min_dist)
                {
                    min_dist = determinant;
                    res = *it;
                    it_delete = it;
                }
            }
            if(res == nullptr) return getIO(head,-1*direction);
            (*pactive).erase(it_delete);
            return res;

    }

};
void simulation(vector<IOrequest*> AllIO,schedmode smode)
{
    IOsched* the_sched;
    if(smode == I) the_sched = new FIFO();
    else if(smode == J) the_sched = new SSTF();
    else if(smode == S) the_sched = new LOOK();
    else if(smode == C) the_sched = new CLOOK();
    else if(smode == F) the_sched = new FLOOK();
    IOrequest* current_IO = nullptr;
    int opindex = 0;
    int head = 0,direction = 1;
    while(true)
    {
        //if new IO arrives.
        if(opindex < AllIO.size() && AllIO[opindex]->arrivetime == current_time)
        {
            the_sched->addIO(AllIO[opindex]);
            opindex++;
        }
        //if current IO is finished, compute and store
        if(current_IO !=nullptr && current_IO->address == head)
        {
            current_IO->endtime = current_time;
            current_IO = nullptr;
        }
        //if no IO is active now
        if(current_IO == nullptr)
        {
            //if we can get, we get next IO
            if(!the_sched->IOque.empty() || !the_sched->addq.empty())
            {
                current_IO = the_sched->getIO(head,direction);
                current_IO->starttime = current_time;
                continue;
            }
            //else if we have reached the end , we stop simulation.
            else if(opindex == AllIO.size()) break;
        }
        //if IO active after all the checks,move head toward the direciton.
        if(current_IO != nullptr )
        {
            //get direction first
            direction = (current_IO->address - head)/abs(current_IO->address - head);
            //then manipulate head with direction
            head+=direction;
            tot_movement++;
        }
        current_time++;

    }
}

int main(int argc,char* argv[])
{
    //schedmode and input file name and schedule character
    schedmode smode;
    char* infname;
    char schedule;
    int GETIN;
    while((GETIN = getopt(argc,argv,"s:")) != -1)
    {
        switch(GETIN)
        {
            case 's':
            if((sscanf(optarg,"%c",&schedule)) <1)
            {
                printf("character after s\n");
                exit(1);
            }
            break;
        }
    }
    if((argc - optind) < 1)
    {
        printf("missing file name\n");
        exit(1);
    }
    //set the schedule mode
    if(schedule == 'i') smode = I;
    else if(schedule == 'j') smode = J;
    else if(schedule == 's') smode = S;
    else if(schedule == 'c') smode = C;
    else if(schedule == 'f') smode = F;
    infname = argv[optind];
    //first get all IO-request into an event queue from input file.
    FILE* fp = fopen(infname,"r");
    char buf[256];
    vector<IOrequest*> AllIO;
    while(fgets(buf,256,fp) != NULL)
    {
        if(buf[0] != '#')
        {
            int temptime,tempaddress;
            sscanf(buf,"%d %d",&temptime,&tempaddress);
            IOrequest* tempIO = new IOrequest(temptime,tempaddress);
            AllIO.push_back(tempIO);
        }
    }
    simulation(AllIO,smode);
    int turnaround=0,waittime=0,max_wt = 0;
    //print IO information
    for(int i =0;i<AllIO.size();i++)
    {
        AllIO[i]->printall();
        turnaround += (AllIO[i]->endtime - AllIO[i]->arrivetime);
        waittime += (AllIO[i]->starttime - AllIO[i]->arrivetime);
        if(max_wt < (AllIO[i]->starttime - AllIO[i]->arrivetime))
        {
            max_wt = AllIO[i]->starttime - AllIO[i]->arrivetime;
        }
    }
    //print sum
    double avg_turnaround = ((double) turnaround)/totalreq;
    double avg_waittime = ((double) waittime)/totalreq;
    printf("SUM: %d %d %.2lf %.2lf %d\n",current_time,tot_movement,avg_turnaround,avg_waittime,max_wt);


}
