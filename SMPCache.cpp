#include "SMPCache.h"

SMPCache::SMPCache(int cpuid, std::vector<SMPCache * > * cacheVector){

    CPUId = cpuid;
    allCaches = cacheVector;

    numReadHits = 0;
    numReadMisses = 0;
    numReadOnInvalidMisses = 0;
    numReadRequestsSent = 0;
    numReadMissesServicedByOthers = 0;
    numReadMissesServicedByShared = 0;
    numReadMissesServicedByModified = 0;

    numWriteHits = 0;
    numWriteMisses = 0;
    numWriteOnSharedMisses = 0;
    numWriteOnInvalidMisses = 0;
    numInvalidatesSent = 0;

}

void SMPCache::conciseDumpStatsToFile(FILE* outFile){

    fprintf(outFile, "-----(CPUId) Cache %lu-----\n", CPUId);

    fprintf(outFile, "  Read hits: %d\n", numReadHits);
    fprintf(outFile, "  Read misses: %d\n", numReadMisses);
    fprintf(outFile, "  Read-on-invalid misses: %d\n", numReadOnInvalidMisses);
    fprintf(outFile, "  Read requests sent: %d\n", numReadRequestsSent);
    fprintf(outFile, "  Rd misses serviced remotely: %d\n", numReadMissesServicedByOthers);
    fprintf(outFile, "  Rd misses serviced by shared: %d\n", numReadMissesServicedByShared);
    fprintf(outFile, "  Rd misses serviced by modified: %d\n", numReadMissesServicedByModified);

    fprintf(outFile, "  Write hits: %d\n", numWriteHits);
    fprintf(outFile, "  Write misses: %d\n", numWriteMisses);
    fprintf(outFile, "  Write-on-shared misses: %d\n", numWriteOnSharedMisses);
    fprintf(outFile, "  Write-on-invalid misses: %d\n", numWriteOnInvalidMisses);
    fprintf(outFile, "  Invalidates sent: %d\n", numInvalidatesSent);

    //  fprintf(outFile,"%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    //                  CPUId,
    //                  numReadHits,
    //                  numReadMisses,
    //                  numReadOnInvalidMisses,
    //                  numReadRequestsSent,
    //                  numReadMissesServicedByOthers,
    //                  numReadMissesServicedByShared,
    //                  numReadMissesServicedByModified,
    //                  numWriteHits,
    //                  numWriteMisses,
    //                  numWriteOnSharedMisses,
    //                  numWriteOnInvalidMisses,
    //                  numInvalidatesSent);

}

void SMPCache::dumpStatsToFile(FILE* outFile){
    fprintf(outFile, "-----Cache %lu-----\n",CPUId);

    fprintf(outFile, "Read Hits:                   %d\n",numReadHits);
    fprintf(outFile, "Read Misses:                 %d\n",numReadMisses);
    fprintf(outFile, "Read-On-Invalid Misses:      %d\n",numReadOnInvalidMisses);
    fprintf(outFile, "Read Requests Sent:          %d\n",numReadRequestsSent);
    fprintf(outFile, "Rd Misses Serviced Remotely: %d\n",numReadMissesServicedByOthers);
    fprintf(outFile, "Rd Misses Serviced by Shared: %d\n",numReadMissesServicedByShared);
    fprintf(outFile, "Rd Misses Serviced by Modified: %d\n",numReadMissesServicedByModified);

    fprintf(outFile, "Write Hits:                  %d\n",numWriteHits);
    fprintf(outFile, "Write Misses:                %d\n",numWriteMisses);
    fprintf(outFile, "Write-On-Shared Misses:      %d\n",numWriteOnSharedMisses);
    fprintf(outFile, "Write-On-Invalid Misses:     %d\n",numWriteOnInvalidMisses);
    fprintf(outFile, "Invalidates Sent:            %d\n",numInvalidatesSent);

}

int SMPCache::getCPUId(){
    return CPUId;
}

int SMPCache::getStateAsInt(unsigned long addr){
    return (int)this->cache->findLine(addr)->getState();
}

std::vector<SMPCache * > *SMPCache::getCacheVector(){
    return allCaches;
}
