#include "pin.H"

#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <dlfcn.h>

#include "MultiCacheSim.h"

std::vector<MultiCacheSim *> Caches;
MultiCacheSim *ReferenceProtocol;
PIN_LOCK globalLock;

bool stopOnError = false;
bool printOnError = false;
bool useRef = false;

KNOB<bool> KnobStopOnError(KNOB_MODE_WRITEONCE, "pintool",
        "stopOnProtoBug", "false", "Stop the Simulation when a deviation is detected between the test protocol and the reference");//default cache is verbose 

KNOB<bool> KnobPrintOnError(KNOB_MODE_WRITEONCE, "pintool",
        "printOnProtoBug", "false", "Print a debugging message when a deviation is detected between the test protocol and the reference");//default cache is verbose 

KNOB<bool> KnobUseReference(KNOB_MODE_WRITEONCE, "pintool",
        "useref", "false", "Use a reference protocol to compare running protocol states with (default = false)");//default cache is verbose 

KNOB<bool> KnobConcise(KNOB_MODE_WRITEONCE, "pintool",
        "concise", "true", "Print output concisely");//default cache is verbose 

KNOB<unsigned int> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool",
        "csize", "65536", "Cache Size");//default cache is 64KB

KNOB<unsigned int> KnobBlockSize(KNOB_MODE_WRITEONCE, "pintool",
        "bsize", "64", "Block Size");//default block is 64B

KNOB<unsigned int> KnobAssoc(KNOB_MODE_WRITEONCE, "pintool",
        "assoc", "2", "Associativity");//default associativity is 2-way

KNOB<unsigned int> KnobNumCaches(KNOB_MODE_WRITEONCE, "pintool",
        "numcaches", "1", "Number of Caches to Simulate");

#ifdef MAC
KNOB<string> KnobProtocol(KNOB_MODE_WRITEONCE, "pintool",
        "protos", "obj-intel64/MSI_SMPCache.dylib", "Cache Coherence Protocol Modules To Simulate");

KNOB<string> KnobReference(KNOB_MODE_WRITEONCE, "pintool",
        "reference", "obj-intel64/MESI_SMPCache.dylib", "Reference Protocol that is compared to test Protocols for Correctness");
#else
KNOB<string> KnobProtocol(KNOB_MODE_WRITEONCE, "pintool",
        "protos", "obj-intel64/MSI_SMPCache.so", "Cache Coherence Protocol Modules To Simulate");

KNOB<string> KnobReference(KNOB_MODE_WRITEONCE, "pintool",
        "reference", "obj-intel64/MESI_SMPCache.so", "Reference Protocol that is compared to test Protocols for Correctness");
#endif

#define MAX_NTHREADS 64
unsigned long instrumentationStatus[MAX_NTHREADS];

enum MemOpType { MemRead = 0, MemWrite = 1 };

INT32 usage()
{
    cerr << "MultiCacheSim -- A Multiprocessor cache simulator with a pin frontend";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}


VOID TurnInstrumentationOn(ADDRINT tid){
    instrumentationStatus[PIN_ThreadId()] = true; 
}

VOID TurnInstrumentationOff(ADDRINT tid){
    instrumentationStatus[PIN_ThreadId()] = false; 
}

// Jiayi, instrument dummy function and print out approximate region
VOID PINTOOL_dummy_approx_region(CHAR * name, VOID * data, unsigned long range, CHAR * data_type)
{
    fprintf(stderr, "PIN approximate region %s wihtout parameters (addr: %p, range %lu, type: %s)\n", name, data, range, data_type);
//    cerr << "PIN approximate region " << name << " wihtout parameters (addr: " << data << ", range: " << range << ", type: " << data_type << endl;
}

VOID instrumentRoutine(RTN rtn, VOID *v){

    if(strstr(RTN_Name(rtn).c_str(),"INSTRUMENT_OFF")){
        RTN_Open(rtn);
        RTN_InsertCall(rtn, 
                IPOINT_BEFORE, 
                (AFUNPTR)TurnInstrumentationOff, 
                IARG_THREAD_ID,
                IARG_END);
        RTN_Close(rtn);
    }


    if(strstr(RTN_Name(rtn).c_str(),"INSTRUMENT_ON")){
        RTN_Open(rtn);
        RTN_InsertCall(rtn, 
                IPOINT_BEFORE, 
                (AFUNPTR)TurnInstrumentationOn, 
                IARG_THREAD_ID,
                IARG_END);
        RTN_Close(rtn);
    }

    if(strstr(RTN_Name(rtn).c_str(), "approx_region")) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                IPOINT_BEFORE,
                (AFUNPTR)PINTOOL_dummy_approx_region,
                IARG_ADDRINT, "dummy_approx_region",
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                IARG_END);
        RTN_Close(rtn);
    }

}

VOID instrumentImage(IMG img, VOID *v)
{
}

void Read(THREADID tid, ADDRINT addr, ADDRINT inst){
    PIN_GetLock(&globalLock, 1);
    if(useRef){
        ReferenceProtocol->readLine(tid,inst,addr);
    }
    std::vector<MultiCacheSim *>::iterator i,e;
    for(i = Caches.begin(), e = Caches.end(); i != e; i++){
        (*i)->readLine(tid,inst,addr);

        if(useRef && (stopOnError || printOnError)){
            if( ReferenceProtocol->getStateAsInt(tid,addr) !=
                    (*i)->getStateAsInt(tid,addr)
              ){
                if(printOnError){
                    fprintf(stderr,"[MCS-Read] State of Protocol %s did not match the reference\nShould have been %d but it was %d\n",
                            (*i)->Identify(),
                            ReferenceProtocol->getStateAsInt(tid,addr),
                            (*i)->getStateAsInt(tid,addr));
                }
                if(stopOnError){
                    exit(1);
                }
            }
        }
    }

    PIN_ReleaseLock(&globalLock);
}

void Write(THREADID tid, ADDRINT addr, ADDRINT inst){
    PIN_GetLock(&globalLock, 1);
    if(useRef){
        ReferenceProtocol->writeLine(tid,inst,addr);
    }
    std::vector<MultiCacheSim *>::iterator i,e;

    for(i = Caches.begin(), e = Caches.end(); i != e; i++){

        (*i)->writeLine(tid,inst,addr);

        if(useRef && (stopOnError || printOnError)){

            if( ReferenceProtocol->getStateAsInt(tid,addr) !=
                    (*i)->getStateAsInt(tid,addr)
              ){
                if(printOnError){
                    fprintf(stderr,"[MCS-Write] State of Protocol %s did not match the reference\nShould have been %d but it was %d\n",
                            (*i)->Identify(),
                            ReferenceProtocol->getStateAsInt(tid,addr),
                            (*i)->getStateAsInt(tid,addr));
                }
                if(stopOnError){
                    exit(1);
                }
            }
        }
    }
    PIN_ReleaseLock(&globalLock);
}

VOID instrumentTrace(TRACE trace, VOID *v)
{

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {  
            if(INS_IsMemoryRead(ins)) {
                INS_InsertCall(ins, 
                        IPOINT_BEFORE, 
                        (AFUNPTR)Read, 
                        IARG_THREAD_ID,
                        IARG_MEMORYREAD_EA,
                        IARG_INST_PTR,
                        IARG_END);
            } else if(INS_IsMemoryWrite(ins)) {
                INS_InsertCall(ins, 
                        IPOINT_BEFORE, 
                        (AFUNPTR)Write, 
                        IARG_THREAD_ID,//thread id
                        IARG_MEMORYWRITE_EA,//address being accessed
                        IARG_INST_PTR,//instruction address of write
                        IARG_END);
            }
        }
    }
}


VOID threadBegin(THREADID threadid, CONTEXT *sp, INT32 flags, VOID *v)
{

}

VOID threadEnd(THREADID threadid, const CONTEXT *sp, INT32 flags, VOID *v)
{

}

VOID dumpInfo(){
}


VOID Fini(INT32 code, VOID *v)
{

    std::vector<MultiCacheSim *>::iterator i,e;
    for(i = Caches.begin(), e = Caches.end(); i != e; i++){
        PIN_GetLock(&globalLock,1);
        (*i)->dumpStatsForAllCaches(KnobConcise.Value());
        PIN_ReleaseLock(&globalLock);
    }

}

BOOL segvHandler(THREADID threadid,INT32 sig,CONTEXT *ctx,BOOL hasHndlr,const EXCEPTION_INFO *pExceptInfo, VOID*v){
    return TRUE;//let the program's handler run too
}

BOOL termHandler(THREADID threadid,INT32 sig,CONTEXT *ctx,BOOL hasHndlr,const EXCEPTION_INFO *pExceptInfo, VOID*v){
    return TRUE;//let the program's handler run too
}


int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) ) {
        return usage();
    }

    PIN_InitLock(&globalLock);

    for(int i = 0; i < MAX_NTHREADS; i++){
        instrumentationStatus[i] = true;
    }

    unsigned long csize = KnobCacheSize.Value();
    unsigned long bsize = KnobBlockSize.Value();
    unsigned long assoc = KnobAssoc.Value();
    unsigned long num = KnobNumCaches.Value();

    const char *pstr = KnobProtocol.Value().c_str();
    char *ct = strtok((char *)pstr,",");
    while(ct != NULL){

        fprintf(stderr,"Opening protocol \"%s\"\n",ct);
        void *chand = dlopen( ct, RTLD_LAZY | RTLD_LOCAL );
        if( chand == NULL ){
            fprintf(stderr,"Couldn't Load %s\n", argv[1]);
            fprintf(stderr,"dlerror: %s\n", dlerror());
            exit(1);
        }

        CacheFactory cfac = (CacheFactory)dlsym(chand, "Create");

        if( chand == NULL ){

            fprintf(stderr,"Couldn't get the Create function\n");
            fprintf(stderr,"dlerror: %s\n", dlerror());
            exit(1);

        }

        MultiCacheSim *c = new MultiCacheSim(stdout, csize, assoc, bsize, cfac);

        for(unsigned int i = 0; i < num; i++){
            c->createNewCache();
        } 

        Caches.push_back(c);

        ct = strtok(NULL,","); 

    }

    useRef = KnobUseReference.Value();
    if(useRef){
        void *chand = dlopen( KnobReference.Value().c_str(), RTLD_LAZY | RTLD_LOCAL );
        if( chand == NULL ){
            fprintf(stderr,"Couldn't Load Reference: %s\n", argv[1]);
            fprintf(stderr,"dlerror: %s\n", dlerror());
            exit(1);
        }

        CacheFactory cfac = (CacheFactory)dlsym(chand, "Create");
        if( chand == NULL ){
            fprintf(stderr,"Couldn't get the Create function\n");
            fprintf(stderr,"dlerror: %s\n", dlerror());
            exit(1);
        }

        ReferenceProtocol = 
            new MultiCacheSim(stdout, csize, assoc, bsize, cfac);

        for(unsigned int i = 0; i < num; i++){

            ReferenceProtocol->createNewCache();

        } 

        fprintf(stderr,"Using Reference Implementation %s\n",KnobReference.Value().c_str());

    }

    stopOnError = KnobStopOnError.Value();
    printOnError = KnobPrintOnError.Value();

    RTN_AddInstrumentFunction(instrumentRoutine,0);
    IMG_AddInstrumentFunction(instrumentImage, 0);
    TRACE_AddInstrumentFunction(instrumentTrace, 0);

    PIN_InterceptSignal(SIGTERM,termHandler,0);
    PIN_InterceptSignal(SIGSEGV,segvHandler,0);

    PIN_AddThreadStartFunction(threadBegin, 0);
    PIN_AddThreadFiniFunction(threadEnd, 0);
    PIN_AddFiniFunction(Fini, 0);

    fprintf(stderr,"Using Protocol %s\n",KnobReference.Value().c_str());

    PIN_StartProgram();

    return 0;
}
