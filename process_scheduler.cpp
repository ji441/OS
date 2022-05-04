#include <iostream>
#include <deque>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <string>
#include <unistd.h>
using namespace std;
vector<int> randvals;
int randlimit;
int current_time=0;
int totalproc=0;
int numio=0;
int iostart=0;
int iototal=0;
enum PSTATE {RUNNING,READY,BLOCKED,CREATED,PREEMPTED,DONE};
vector<char*> pstatevec ={"RUNNING","READY","BLOCKED","CREATED","PREEMPTED","DONE"};
enum Transition {toready,torun,toblock,topreempt};
vector<char*> eventrans = {"toready","torun","toblock","topreempt"};
enum schedmode {F,L,S,R,P,E};
vector<char*> smodevec = {"FCFS","LCFS","SRTF","RR","PRIO","PREPRIO"};
/*the index of randvals, starting with zero*/
int ofs=0;
class process
{
public:
    int AT,TC,CB,IO,PRIO;
    /*cb: cpu burst, io burst,dynamic priority,remaining time,current state,state time*/
    int cb,ib,dprio,rem,state,state_ts;
    int FT=0,TT=0,IT=0,CW=0;
    int proid;
    process()
    {
        totalproc++;
        proid=totalproc-1;
    }
};
class event
{
    public:
    int TS,trans;
    process* proc;
};
deque<event*> eq;
event* get_event()
{
    if(eq.empty()) return nullptr;
    event* evt= eq.front();
    eq.pop_front();
    return evt;
}
void put_event(event* evt)
{
    for(deque<event*>::iterator it=eq.begin();it !=eq.end();++it){
        int newtime = evt->TS;
        int positiontime = (*it)->TS;
        if(newtime < positiontime)
        {
            eq.insert(it,evt);
            return;
        }

    }
    /*if not strictly smaller than any of the event in queue, put it at the end of the queue*/
    eq.push_back(evt);
    return;
}
/*define base class of schedule*/
class schedule
{
public:
    /*all scheduler will need a runque*/
    deque<process*> runque;
    virtual void add_proc(process* p)
    {
        return;
    }
    virtual process* get_next_proc(){
        return nullptr;
    }
    virtual bool test_preempt(process* p,int curtime){
        return true;
    }
    virtual bool test_prioPre(process* rdyp, process* crunp){
        return false;
    }
    /*trace runque function*/
    void traceRQ(){
        cout<<"rq:\n";
        for(deque<process*>::iterator it=runque.begin();it!=runque.end();++it){
            int prid = (*it)->proid;
            int remt = (*it)->rem;
            printf("%d %d\n",prid,remt);
        }
        cout<<"\n";
    }

};
class FIFO : public schedule
{
public:
    void add_proc(process* p){
        runque.push_back(p);
    }
    process* get_next_proc(){
        if(runque.empty()) return nullptr;
        process* temp=runque.front();
        runque.pop_front();
        return temp;
    }
    bool test_preempt(process*p, int curtime){
        /*since in FIFO there is no preempt, so always return false.*/
        return false;
    }

};
class LCFS : public schedule
{
public:
    void add_proc(process* p){
        runque.push_front(p);
    }
    process* get_next_proc(){
        if(runque.empty()) return nullptr;
        process* temp=runque.front();
        runque.pop_front();
        return temp;
    }
    bool test_preempt(process*p, int curtime){
        /*since in LCFS there is no preempt, so always return false.*/
        return false;
    }

};
class SRTF : public schedule
{
public:
    void add_proc(process* p){
        for(deque<process*>::iterator it=runque.begin();it!=runque.end();++it){
            int newrem=p->rem;
            int positionrem=(*it)->rem;
            if(newrem < positionrem){
                runque.insert(it,p);
                return;
            }
        }
        /*if no strictly smaller than any rem time in the queue, put it to the back*/
        runque.push_back(p);
        return;
    }
    process* get_next_proc(){
        if(runque.empty()) return nullptr;
        process* temp=runque.front();
        runque.pop_front();
        return temp;
    }
    bool test_preempt(process*p, int curtime){
        /*since in LCFS there is no preempt, so always return false.*/
        return false;
    }
};
class RR : public schedule
{
public:
    void add_proc(process* p){
        runque.push_back(p);
    }
    process* get_next_proc(){
        if(runque.empty()) return nullptr;
        process* temp=runque.front();
        runque.pop_front();
        return temp;
    }
    bool test_preempt(process*p, int quantum){
        if(p->cb > quantum) return true;
        return false;
    }
};
class PRIO : public schedule
{
public:
    int maxprio;
    vector<deque<process*>> activeQ;
    vector<deque<process*>> expiredQ;
    vector<deque<process*>>* pactive = &activeQ;
    vector<deque<process*>>* pexpired = &expiredQ;
    PRIO()
    {

    }
    PRIO(int x)
    {
        maxprio = x;
        for(int i = 0;i<maxprio;i++){
            deque<process*> * ptr1 = new deque<process*>();
            activeQ.push_back(*ptr1);
            deque<process*> * ptr2 = new deque<process*>();
            expiredQ.push_back(*ptr2);
        }
    }
    void add_proc(process* p)
    {
        int priority = p->dprio;
        if(priority == -1){
            /*if negative 1, reset to staticprio-1*/
            p->dprio = p->PRIO -1;
            (*pexpired)[p->dprio].push_back(p);
        }
        else (*pactive)[priority].push_back(p);
    }
    process* get_next_proc()
    {
        for(int i =maxprio-1;i>=0;i--){
            if(!(*pactive)[i].empty()){
                process* temp = (*pactive)[i].front();
                (*pactive)[i].pop_front();
                return temp;
            }
        }
        /*if we did not get anything in the activeQ, swap pointer and try again.*/
        vector<deque<process*>>* ptemp =  pactive;
        pactive = pexpired;
        pexpired = ptemp;
        for(int i =maxprio-1;i>=0;i--){
            if(!(*pactive)[i].empty()){
                process* temp = (*pactive)[i].front();
                (*pactive)[i].pop_front();
                return temp;
            }
        }
        /*if we do not find anything in expiredQ either, return null.*/
        return nullptr;
    }
    bool test_preempt(process*p, int quantum){
        if(p->cb > quantum) return true;
        return false;
    }

};
class prePRIO : public PRIO
{
public:
    prePRIO(int x)
    {
        maxprio = x;
        for(int i = 0;i<maxprio;i++){
            deque<process*> * ptr1 = new deque<process*>();
            activeQ.push_back(*ptr1);
            deque<process*> * ptr2 = new deque<process*>();
            expiredQ.push_back(*ptr2);
        }
    }
    bool test_prioPre(process* rdyp, process* crunp)
    {
        /*no current running process no need for preempt.*/
        if(crunp == nullptr) return false;
        /*if running process have equal or higher priority than the ready one, we keep running*/
        if(crunp->dprio >= rdyp->dprio) return false;
        /*then look at event queue, to search for event with current time for the running process*/
        deque<event*>::iterator it = eq.begin();
        while((*it)->TS == current_time){
            /*if we find event at currenttime for running process we do not preempt*/
            if(((*it)->proc)->proid == crunp->proid){
                return false;
            }
            ++it;
        }
        /*else we preempt.*/
        return true;
    }

};
int myrandom(int burst)
{
    if(ofs == randlimit) ofs=0;
    return 1+(randvals[ofs++] % burst);
}
int get_next_event_time()
{
    if(eq.empty()) return -1;
    event* evt = eq.front();
    return evt->TS;

}
void traceEQ(){
    cout<<"eq:\n";
    for(deque<event*>::iterator it=eq.begin();it!=eq.end();++it){
        int prid=((*it)->proc)->proid;
        int ti=(*it)->TS;
        char* tranf = eventrans[(*it)->trans];
        printf("%d %d %s\n",ti,prid,tranf);
    }
    cout<<'\n';

}
void Simulation(schedmode schedstate,int quantum,int maxprio)
{
    /*create different schedule according to schedstate*/
    schedule* the_sched;
    if(schedstate == F) the_sched = new FIFO();
    else if(schedstate == L) the_sched = new LCFS();
    else if(schedstate == S) the_sched = new SRTF();
    else if(schedstate == R) the_sched = new RR();
    else if(schedstate == P) the_sched = new PRIO(maxprio);
    else if(schedstate == E) the_sched = new prePRIO(maxprio);
    event* evt;
    process* crunproc = nullptr;
    while(evt=get_event()){
        bool callsched = false;

        process* curproc = evt->proc;
        current_time = evt->TS;
        int currentid = curproc->proid;
        int transi = evt->trans;
        int timepre= current_time-curproc->state_ts;
        delete evt;
        evt=nullptr;
        switch(transi){
        case toready:
        {
            /*update iotime and priority when comming back from blocked.*/
            if(curproc->state == BLOCKED){
                curproc->dprio = curproc->PRIO -1;
                curproc->IT += timepre;
                if(numio ==1) iototal += current_time-iostart;
                numio--;
            }
            /*test for priority preempt*/
            if(the_sched->test_prioPre(curproc,crunproc)){
                deque<event*>::iterator it1 = eq.begin();
                /*finding the next event for current running process,remove it, and create preempt event now*/
                while(((*it1)->proc)->proid != crunproc->proid) ++it1;
                event* revt = *it1;
                deque<event*>::iterator it2 = it1;
                ++it2;
                eq.erase(it1,it2);
                delete revt;
                revt = nullptr;
                event* evt = new event();
                evt->TS = current_time;
                evt->proc = crunproc;
                evt->trans = topreempt;
                put_event(evt);
                /*finally reset the crunproc to null*/
                crunproc = nullptr;
            }
            /*printf("%d %d %d: %s to READY\n",current_time,currentid,timepre,pstatevec[curproc->state]);*/
            curproc->state = READY;
            curproc->state_ts=current_time;
            the_sched->add_proc(curproc);
            callsched = true;
            break;
        }
        case torun:
        {
            crunproc = curproc;
            /*update state information and waiting time*/
            curproc->CW += timepre;
            /*get new random cpu burst in this transition, when the process is not from a preempt*/
            if(curproc->state != PREEMPTED) curproc->cb=myrandom(curproc->CB);
            if(curproc->rem < curproc->cb) curproc->cb=curproc->rem;
            /*printf("%d %d %d: %s -> RUNNG cb=%d rem=%d prio=%d\n",current_time,currentid,timepre,pstatevec[curproc->state],curproc->cb,curproc->rem,curproc->dprio);*/
            curproc->state = RUNNING;
            curproc->state_ts = current_time;
            /*check for preempt*/
            if(the_sched->test_preempt(curproc,quantum)){

                event* nevt = new event();
                nevt->TS = current_time+quantum;
                nevt->trans = topreempt;
                nevt->proc = curproc;
                put_event(nevt);
            }
            /*check for block*/
            else {
                event* nevt = new event();
                nevt->TS = current_time + curproc->cb;
                nevt->trans = toblock;
                nevt->proc = curproc;
                put_event(nevt);

            }
            break;
        }
        case toblock:
        {
            /*set current running process to nullptr, since it is blocked*/
            crunproc = nullptr;
            callsched = true;
            curproc->cb -= timepre;
            curproc->rem -= timepre;
            /*check for event done*/
            if(curproc->rem == 0){
                curproc->state = DONE;
                curproc->FT = current_time;
                curproc->TT = curproc->FT - curproc->AT;
                /*printf("%d %d %d: DONE\n",current_time,currentid,timepre);*/
                break;
            }
            /*if no io before, get iostart*/
            if(numio==0) iostart = current_time;
            /*increase number of io*/
            numio++;
            curproc->ib = myrandom(curproc->IO);
            /*printf("%d %d %d: %s to BLOCK ib=%d rem=%d\n",current_time,currentid,timepre,pstatevec[curproc->state],curproc->ib,curproc->rem);*/
            curproc->state = BLOCKED;
            curproc->state_ts = current_time;
            /*now creating new event when it becomes ready again*/
            event* nevt = new event();
            nevt->TS = current_time + curproc->ib;
            nevt->trans = toready;
            nevt->proc = curproc;
            put_event(nevt);
            break;
        }
        case topreempt:
        {
            curproc->cb -= timepre;
            curproc->rem -= timepre;
            crunproc = nullptr;
            /*printf("%d %d %d: %s to preempt ib=%d rem=%d\n",current_time,currentid,timepre,pstatevec[curproc->state],curproc->ib,curproc->rem);*/
            curproc->state = PREEMPTED;
            curproc->state_ts = current_time;
            curproc->dprio -=1;
            the_sched->add_proc(curproc);
            callsched = true;
            break;
        }
        }
        if(callsched){
            if(get_next_event_time() == current_time){
                continue;
            }
            callsched = false;
            if(crunproc == nullptr){
                crunproc = the_sched->get_next_proc();
                /*if no more process in runque then we are done for now, and move to events at future time.*/
                if(crunproc == nullptr){
                    continue;
                }
                /*if there is still process need to run at this time create event for it.*/
                event* nevt=new event();
                nevt->proc = crunproc;
                nevt->trans = torun;
                nevt->TS = current_time;
                put_event(nevt);
            }
        }


    }
}
int main(int argc, char* argv[])
{
    int quantum=2;
    int maxprio=4;
    char* fname;
    char* rfname;
    char smode;
    int index;
    int C;
    schedmode schedstate;
    while((C=getopt(argc,argv,"s:"))!= -1)
    {
        switch(C)
        {
        case 's':
            int i = sscanf(optarg,"%c%d:%d",&smode,&quantum,&maxprio);
            if(i<1){
                printf("syntax error\n");
                exit(1);
            }
            break;
        }


    }
    if(optind >= argc-1)
    {
        printf("syntax error\n");
        exit(1);
    }
    if(smode == 'F') schedstate = F;
    else if (smode == 'L') schedstate = L;
    else if (smode == 'S') schedstate = S;
    else if (smode == 'R') schedstate = R;
    else if (smode == 'P') schedstate = P;
    else if (smode == 'E') schedstate = E;
    else
    {
        cout<<"syntax error\n";
        exit(1);
    }
    fname = argv[optind];
    rfname = argv[++optind];
    FILE* fp=fopen(fname,"r");
    /*open the rfile*/
    FILE* rf=fopen(rfname,"r");
    char randbuf[256];
    fgets(randbuf,256,rf);
    randlimit = atoi(strtok(randbuf," \n"));
    /*get all randvals from the rfile*/
    while(fgets(randbuf,256,rf) != NULL){
        char* tok=strtok(randbuf," \n");
        int randnum= atoi(tok);
        randvals.push_back(randnum);
    }
    /*record all process so that at last we can refer to them and printout relevant message*/
    deque<process*> procq;
    char linebuf[256];
    /*get all process,put into created in event queue*/
    while(fgets(linebuf,256,fp) != NULL){
        char* tok=strtok(linebuf," ");
        if(tok==NULL) continue;
        process* temp = new process();
        temp->AT = atoi(tok);
        tok=strtok(NULL," ");
        temp->TC= atoi(tok);
        tok=strtok(NULL," ");
        temp->CB= atoi(tok);
        tok=strtok(NULL," ");
        temp->IO= atoi(tok);
        temp->rem=temp->TC;
        temp->state=CREATED;
        temp->state_ts=temp->AT;
        /*get static priority*/
        temp->PRIO=myrandom(maxprio);
        /*initialize dynamic priority*/
        temp->dprio= temp->PRIO-1;
        event* tevent= new event();
        tevent->proc = temp;
        tevent->TS = temp->AT;
        tevent->trans = toready;
        eq.push_back(tevent);
        procq.push_back(temp);
    }
    Simulation(schedstate,quantum,maxprio);
    double sumTC = 0,sumTT=0, sumCW=0;
    if(schedstate == F ||schedstate == L||schedstate == S) printf("%s\n",smodevec[schedstate]);
    else printf("%s %d\n",smodevec[schedstate],quantum);
    for(deque<process*>::iterator it=procq.begin();it!=procq.end();++it){
        int pid,AT,TC,CB,IO,PRIO,FT,TT,IT,CW;
        pid= (*it)->proid;
        AT= (*it)->AT;
        TC= (*it)->TC;
        CB= (*it)->CB;
        IO= (*it)->IO;
        PRIO= (*it)->PRIO;
        FT= (*it)->FT;
        TT= (*it)->TT;
        IT= (*it)->IT;
        CW= (*it)->CW;
        sumTC += TC;
        sumTT += TT;
        sumCW += CW;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",pid,AT,TC,CB,IO,PRIO,FT,TT,IT,CW);
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",current_time,100*sumTC/current_time,100*(double)iototal/current_time,sumTT/totalproc,sumCW/totalproc,100*(double)totalproc/current_time);

}
