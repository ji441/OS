#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <utility>
#include<iomanip>
using namespace std;

FILE* fp;
int newline=1;
int linenum=0;
int offset=0;
char linebuffer[256];
int linelen=0;
char* tok=NULL;
void parseerror(int errocode){
    vector<string> errstr={
    "NUM_EXPECTED",
    "SYM_EXPECTED",
    "ADDR_EXPECTED",
    "SYM_TOO_LONG",
    "TOO_MANY_DEF_IN_MODULE",
    "TOO_MANY_USE_IN_MODULE",
    "TOO_MANY_INSTR"
    };
    cout<<"Parse Error line "<<linenum<<" offset "<<offset<<": "<<errstr[errocode]<<"\n";
}
/*get token one at a time and update the global address*/

char* getToken(){
    if(newline==1){
        /*need a newline but already get to the eof*/
        if(fgets(linebuffer,256,fp)==NULL){
            offset=linelen;
            return NULL;
        }
        /*can get a new line*/
        linelen=strlen(linebuffer);
        offset=1;
        linenum++;
        /*then new line is not empty.we try to get token*/
        tok=strtok(linebuffer," \t\n");
        /*if no token in the new line, go to next line*/
        if(tok==NULL) return getToken();
        /*then we have get a token*/
        newline=0;
        offset=tok-linebuffer+1;
        return tok;
    }
    else if(newline==0){
        /*when newline is not needed, try get a token from linebuf*/
        tok=strtok(NULL," \t\n");
        /*if get a NULL token,means current line has been exhausted,go to nextline*/
        if(tok==NULL){
            newline=1;
            return getToken();
        }
        /*if tok is not NULL, just output it*/
        offset=tok-linebuffer+1;
        return tok;
    }
}
int readInt(int argument=0){
    char* Itok=getToken();
    if(Itok==NULL){
        if(argument==1) return -1;
        parseerror(0);
        exit(99);
    }
    string s(Itok);
    /*check for real integer*/
    for(int i=0;i<s.size();i++){
        if(isdigit(s[i])==0){

            parseerror(0);
            exit(99);
        }
    }
    return stoi(s);
}
string readSym(){
    char* Stok=getToken();
    if(Stok==NULL){
        parseerror(1);
        exit(99);
    }
    string s(Stok);
    if(s.size()>16){
        parseerror(3);
        exit(99);
    }
    if(!isalpha(s[0])){
        parseerror(1);
        exit(99);
    }
    for(int i=0;i<s.size();i++){
        if(!isalnum(s[i])){
            parseerror(1);
            exit(99);
        }
    }
    return s;
}
string readIEAR(){
    char* Atok=getToken();

    if(Atok==NULL){
        parseerror(2);
        exit(99);
    }
    string s(Atok);
    if(s=="I"||s=="E"||s=="A"||s=="R") return s;
    parseerror(2);
    exit(99);
}
int main(int argc,char* argv[])
{
    /*using vector of pairs to store symbol table*/
    vector<pair<string,int>> defsym;
    /*store the index in symbol table that is being defined multi times*/
    vector<int> multidef;
    /*record the modnumber of each sym defined*/
    vector<int> defsymmod;
    /*current mod base index*/
    int modbase=0;
    int modnumber=0;
    char* fname=argv[1];
    fp=fopen(fname,"r");
    if(fp==NULL) {
        printf("no such file\n");
        exit(99);
    }
    /*pass 1*/
    while(!feof(fp)){
        int defcount = readInt(1);
        if(defcount == -1) break;
        /*check for too many definition.*/
        if(defcount>16){
            parseerror(4);
            exit(99);
        }

        modnumber++;
        /*collect all symbols according to defcount*/
        for(int i=0;i<defcount;i++){
            int key=1;
            string sym=readSym();
            int val=readInt();
            for(int j=0;j<defsym.size();j++){
                if(sym==defsym[j].first){
                    multidef.push_back(j);
                    key=0;
                }
            }
            if(key==1){
                pair<string,int> newsym;
                newsym.first=sym;
                newsym.second=val+modbase;
                defsym.push_back(newsym);
                defsymmod.push_back(modnumber);
            }
        }
        int usecount=readInt();
        /*check for too many uselists*/
        if(usecount>16){
            parseerror(5);
            exit(99);
        }
        /*read those use tokens but do nothing in pass1*/
        for(int i=0;i<usecount;i++){
            string sym=readSym();
        }
        int instcount=readInt();
        /*check for too many instruction*/

        if(instcount+modbase>512){
            parseerror(6);
            exit(99);
        }

        for(int i=0;i<instcount;i++){
            string addressmode=readIEAR();
            int operand = readInt();
        }
        /*check for error 5, define variable that exceed current modsize*/
        for(int i=0;i<defsym.size();i++){
            if(defsym[i].second >= (modbase+instcount)){
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n",modnumber,defsym[i].first.c_str(),defsym[i].second-modbase,instcount-1);
                defsym[i].second=modbase;
            }
        }
        modbase+=instcount;
    }
    /*print out all defined symbol*/
    cout<<"Symbol Table\n";
    for(int i=0;i<defsym.size();i++){
        int key=1;
        for(int j=0;j<multidef.size();j++){
            if(i==multidef[j]) key=0;
        }
        if(key==0){
            cout<<defsym[i].first<<"="<<defsym[i].second<<" Error: This variable is multiple times defined; first value used\n";
        }
        else{
            cout<<defsym[i].first<<"="<<defsym[i].second<<"\n";
        }
    }
    cout<<"\n";
    cout<<"Memory Map\n";
    /*pass 1 finished, reopen file and reset tok, linenum, offset newline,modnumber and modbase*/
    linenum=0;
    offset=0;
    tok=NULL;
    newline=1;
    fclose(fp);
    modnumber=0;
    modbase=0;
    fp=fopen(fname,"r");
    /*pass 2 begins,create a vector store use condition of all defined symbol*/
    vector<int> usedefsym(defsym.size(),0);
    while(!feof(fp)){
        /*read defcount and all symbols but do nothing since work is done in pass1*/
        int defcount = readInt(1);
        if(defcount == -1) break;
        modnumber++;
        for(int i=0;i<defcount;i++){
            readSym();
            readInt();
        }
        /*read use list*/
        int usecount= readInt();
        /*store uselist sym and their use condition*/
        vector<pair<string,int>> usesym;
        for(int i=0;i<usecount;i++){
            pair<string,int> newsym(readSym(),0);
            usesym.push_back(newsym);
        }
        /*begin to deal with instruction part*/
        int instcount= readInt();
        for(int i=0;i<instcount;i++){
            string addressmode = readIEAR();
            int operand=readInt();
            int opcode=operand/1000;
            int address=operand % 1000;
            /*condition with I*/
            if(addressmode=="I"){
                if(operand>=10000){
                    operand=9999;
                    cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<operand<<" Error: Illegal immediate value; treated as 9999\n";
                }
                else cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<"\n";
                continue;
            }
            if(opcode>=10){
                operand=9999;
                cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<operand<<" Error: Illegal opcode; treated as 9999\n";
                continue;
            }
            /*condition with A*/
            if(addressmode=="A"){
                if(address>512){
                    address=0;
                    operand=opcode*1000;
                    cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<" Error: Absolute address exceeds machine size; zero used\n";
                }
                else cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<"\n";
            }
            /*condition with R*/
            if(addressmode=="R"){
                if(address >= instcount){
                    operand=opcode*1000+modbase;
                    cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<" Error: Relative address exceeds module size; zero used\n";
                }
                else cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand+modbase<<"\n";
            }
            /*condition with E*/
            if(addressmode=="E"){
                /*get the reference in uselist*/
                if(address >= usesym.size()){
                    cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<" Error: External address exceeds length of uselist; treated as immediate\n";
                    continue;
		}
                /*get it from uselist*/
                string esym = usesym[address].first;
                usesym[address].second=1;
                /*search in deflist*/
                int key=0;
                for(int j=0;j<defsym.size();j++){
                    if(esym==defsym[j].first){
                        usedefsym[j]=1;
                        operand=opcode*1000+defsym[j].second;
                        key=1;
                        cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<"\n";
                        break;
                    }
                }
                if(key==0){
                    operand=opcode*1000;
                    cout<<setw(3)<<setfill('0')<<modbase+i<<": "<<setw(4)<<setfill('0')<<operand<<" Error: "<<esym<<" is not defined; zero used\n";
                }
            }

        }
        /*rule 7 search for unused in uselist*/
        for(int i=0;i<usesym.size();i++){
            if(usesym[i].second==0){
                printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n",modnumber,usesym[i].first.c_str());
            }
        }
        modbase+=instcount;
    }
    /*finally rule 4 defined but not used sym*/
    cout<<"\n";
    for(int i=0;i<usedefsym.size();i++){
        if(usedefsym[i]==0){
            printf("Warning: Module %d: %s was defined but never used\n",defsymmod[i],defsym[i].first.c_str());
        }
    }
}
