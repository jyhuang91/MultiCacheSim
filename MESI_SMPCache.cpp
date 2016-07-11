#include "MESI_SMPCache.h"

MESI_SMPCache::MESI_SMPCache(int cpuid, std::vector<SMPCache * > * cacheVector,
                 int csize, int cassoc, int cbsize, int caddressable, const char * repPol, bool cskew) : 
               SMPCache(cpuid,cacheVector){
  fprintf(stderr,"Making a MESI cache with cpuid %d\n",cpuid);
  CacheGeneric<MESI_SMPCacheState> *c = CacheGeneric<MESI_SMPCacheState>::create(csize, cassoc, cbsize, caddressable, repPol, cskew);
  cache = (CacheGeneric<StateGeneric<> >*)c; 

}

void MESI_SMPCache::fillLine(uint32_t addr, uint32_t mesi_state){
  MESI_SMPCacheState *st = (MESI_SMPCacheState *)cache->findLine2Replace(addr); //this gets the contents of whateverline this would go into
  if(st==0){
    return;
  }
  st->setTag(cache->calcTag(addr));
  st->changeStateTo((MESIState_t)mesi_state);
  return;
    
}
  

MESI_SMPCache::RemoteReadService MESI_SMPCache::readRemoteAction(uint32_t addr){

  std::vector<SMPCache * >::iterator cacheIter;
  std::vector<SMPCache * >::iterator lastCacheIter;
  for(cacheIter = this->getCacheVector()->begin(), lastCacheIter = this->getCacheVector()->end(); cacheIter != lastCacheIter; cacheIter++){
    MESI_SMPCache *otherCache = (MESI_SMPCache*)*cacheIter; 

    if(otherCache->getCPUId() == this->getCPUId()){
      continue;
    }
      
    MESI_SMPCacheState* otherState = (MESI_SMPCacheState *)otherCache->cache->findLine(addr);
    if(otherState){
      if(otherState->getState() == MESI_MODIFIED){

        otherState->changeStateTo(MESI_SHARED);
        return MESI_SMPCache::RemoteReadService(false,true);

      }else if(otherState->getState() == MESI_EXCLUSIVE){

        otherState->changeStateTo(MESI_SHARED); 
        return MESI_SMPCache::RemoteReadService(false,true);

      }else if(otherState->getState() == MESI_SHARED){  //doesn't matter except that someone's got it

        return MESI_SMPCache::RemoteReadService(true,true);

      }else if(otherState->getState() == MESI_INVALID){ //doesn't matter at all.

      }
    }

  }//done with other caches

  //fprintf(stderr,"Done with all caches\n");
  //This happens if everyone was MESI_INVALID
  return MESI_SMPCache::RemoteReadService(false,false);
}

void MESI_SMPCache::readLine(uint32_t rdPC, uint32_t addr, bool approx){

  MESI_SMPCacheState *st = (MESI_SMPCacheState *)cache->findLine(addr);    
  //fprintf(stderr,"In MESI ReadLine\n");
  if(!st || (st && !(st->isValid())) ){//Read Miss -- i need to look in other peoples' caches for this data
    
    numReadMisses++;


    if(st){
      numReadOnInvalidMisses++;
    }

    //Query the other caches and get a remote read service object.
    MESI_SMPCache::RemoteReadService rrs = readRemoteAction(addr);
    numReadRequestsSent++;
      
    MESIState_t newMesiState = MESI_INVALID;
  
    if(rrs.providedData){
   
      numReadMissesServicedByOthers++;

      if(rrs.isShared){
 
        numReadMissesServicedByShared++;
         
      }else{ 
      
        numReadMissesServicedByModified++;
      } 

      newMesiState = MESI_SHARED;

    }else{

      newMesiState = MESI_EXCLUSIVE;

    }
    //Fill the line
    //fprintf(stderr,"MESI ReadLine: Miss -- calling fill line\n");
    fillLine(addr,newMesiState); 

      
  }else{ //Read Hit

    numReadHits++; 
    return; 

  }

}


MESI_SMPCache::InvalidateReply MESI_SMPCache::writeRemoteAction(uint32_t addr){
    
    bool empty = true;
    std::vector<SMPCache * >::iterator cacheIter;
    std::vector<SMPCache * >::iterator lastCacheIter;
    for(cacheIter = this->getCacheVector()->begin(), lastCacheIter = this->getCacheVector()->end(); cacheIter != lastCacheIter; cacheIter++){
      MESI_SMPCache *otherCache = (MESI_SMPCache*)*cacheIter; 
      if(otherCache->getCPUId() == this->getCPUId()){
        continue;
      }

      //Get the line from the current other cache 
      MESI_SMPCacheState* otherState = (MESI_SMPCacheState *)otherCache->cache->findLine(addr);

      //if it was actually in the other cache:
      if(otherState && otherState->isValid()){
          /*Invalidate the line, cause we're writing*/
          otherState->invalidate();
          empty = false;
      }

    }//done with other caches

    //Empty=true indicates that no other cache 
    //had the line or there were no other caches
    //
    //This data in this object is not used as is, 
    //but it might be useful if you plan to extend 
    //this simulator, so i left it in.
    return MESI_SMPCache::InvalidateReply(empty);
}


void MESI_SMPCache::writeLine(uint32_t wrPC, uint32_t addr, bool approx){

  MESI_SMPCacheState * st = (MESI_SMPCacheState *)cache->findLine(addr);    
    
  if(!st || (st && !(st->isValid())) ){ //Write Miss
    
    numWriteMisses++;
  
    if(st){
      numWriteOnInvalidMisses++;
    }
  
    //MESI_SMPCache::InvalidateReply inv_ack = writeRemoteAction(addr);
    writeRemoteAction(addr);
    numInvalidatesSent++;

    //Fill the line with the new write
    fillLine(addr,MESI_MODIFIED);
    return;

  }else if(st->getState() == MESI_SHARED ||
           st->getState() == MESI_EXCLUSIVE){ //Coherence Miss
    
    numWriteMisses++;
    numWriteOnSharedMisses++;
      
    //MESI_SMPCache::InvalidateReply inv_ack = writeRemoteAction(addr);
    writeRemoteAction(addr);
    numInvalidatesSent++;

    st->changeStateTo(MESI_MODIFIED);
    return;

  }else{ //Write Hit

    numWriteHits++;
    return;

  }

}

MESI_SMPCache::~MESI_SMPCache(){

}

char *MESI_SMPCache::Identify(){
  return (char *)"MESI Cache Coherence";
}


extern "C" SMPCache *Create(int num, std::vector<SMPCache*> *cvec, int csize, int casso, int bs, int addrble, const char *repl, bool skw){

  return new MESI_SMPCache(num,cvec,csize,casso,bs,addrble,repl,skw);

}
