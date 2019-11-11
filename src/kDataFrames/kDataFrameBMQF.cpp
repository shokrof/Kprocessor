#include "kDataFrame.hpp"
#include "Utils/kmer.h"
#include <iostream>
#include <fstream>
#include <math.h>
#include <limits>
#include <sstream>


kDataFrameBMQF::kDataFrameBMQF():kDataFrame(){
    bufferedmqf=new bufferedMQF();
    bufferedMQF_init(bufferedmqf, (1ULL<<16), (1ULL<<16), 2*kSize, 0,2, "");
    KD = (new Kmers(kSize));
    falsePositiveRate=0;
    hashbits=2*kSize;
    range=(1ULL<<hashbits);
}

kDataFrameBMQF::kDataFrameBMQF(uint64_t ksize,uint8_t q,uint8_t fixedCounterSize,uint8_t value_bits,double falsePositiveRate):
        kDataFrame(ksize){
    bufferedmqf=new bufferedMQF();
    bufferedMQF_init(bufferedmqf, (1ULL<<q-2), (1ULL<<q), 2*kSize, value_bits,fixedCounterSize, "");
    this->falsePositiveRate=falsePositiveRate;
    if(falsePositiveRate==0){
        KD = (new Kmers(kSize));
    }
    else if(falsePositiveRate<1){
        KD = (new Kmers(kSize,0));
    }
    hashbits=2*kSize;
    range=(1ULL<<hashbits);
}


kDataFrameBMQF::kDataFrameBMQF(uint64_t ksize):
        kDataFrame(ksize){
    this->falsePositiveRate=0.0;
    KD = (new Kmers(kSize));
    hashbits=2*kSize;
    range=(1ULL<<hashbits);
    bufferedmqf=NULL;
    reserve(10000);
}

kDataFrameBMQF::kDataFrameBMQF(bufferedMQF* bufferedmqf,uint64_t ksize,double falsePositiveRate):
        kDataFrame(ksize)
{
    this->bufferedmqf=bufferedmqf;
    this->falsePositiveRate=falsePositiveRate;
    if(falsePositiveRate==0){
        KD = (new Kmers(kSize));
    }
    else if(falsePositiveRate<1){
        KD = (new Kmers(kSize,0));
    }
    hashbits=this->bufferedmqf->memoryBuffer->metadata->key_bits;
    hashbits=2*kSize;
    range=(1ULL<<hashbits);
}

kDataFrame* kDataFrameBMQF::getTwin(){
    uint64_t q=log2(bufferedmqf->disk->metadata->nslots);
    return ((kDataFrame*)new kDataFrameBMQF(kSize,q,bufferedmqf->memoryBuffer->metadata->fixed_counter_size,
                                            bufferedmqf->memoryBuffer->metadata->label_bits,falsePositiveRate));
}

void kDataFrameBMQF::reserve(uint64_t n)
{
    bufferedMQF* old=bufferedmqf;
    bufferedmqf=new bufferedMQF();
    uint64_t q=(uint64_t)ceil(log2((double)n*1.4));
    bufferedMQF_init(bufferedmqf, (1ULL<<(q-2)), (1ULL<<q), hashbits, 0,2, "");
    if(old!=NULL)
    {
        // ERROR FLAG: bufferedMQF_copy(bufferedmqf,old)
        bufferedMQF_copy(bufferedmqf,old);
        bufferedMQF_destroy(old);
        delete old;
    }
}
void kDataFrameBMQF::reserve(vector<uint64_t> countHistogram) {
    bufferedMQF* old=bufferedmqf;
    bufferedmqf=new bufferedMQF();
    uint64_t nSlots;
    uint64_t fixedCounterSize;
    uint64_t memory;
    kDataFrameMQF::estimateParameters(countHistogram, 2 * getkSize(), 0,
                                      &nSlots, &fixedCounterSize, &memory);
//    std::cerr << "[DEBUG] Q: " << q << std::endl;
    bufferedMQF_init(bufferedmqf, nSlots/2, nSlots, hashbits, 0,2, "");
    if (old != NULL) {
        bufferedMQF_migrate(old, bufferedmqf);
       // qf_destroy(old);
        delete old;
    }
}
kDataFrameBMQF::kDataFrameBMQF(uint64_t ksize,vector<uint64_t> countHistogram,uint8_t value_bits
        ,double falsePositiveRate):
        kDataFrame(ksize)
{
    uint64_t nSlots;
    uint64_t fixedCounterSize;
    uint64_t memory;
    kDataFrameBMQF::estimateParameters(countHistogram,2*ksize,value_bits,
                                       &nSlots,&fixedCounterSize,&memory);
    // bufferedMQF_init requires different arguments
    bufferedMQF_init(bufferedmqf, nSlots, 2*ksize,value_bits,fixedCounterSize, 2,"");
}

uint64_t kDataFrameBMQF::estimateMemory(uint64_t nslots,uint64_t slotSize, uint64_t fcounter, uint64_t value_bits)
{
    uint64_t SLOTS_PER_BLOCK_t=64;
    uint64_t xnslots = nslots + 10*sqrt((double)nslots);
    uint64_t nblocks = (xnslots + SLOTS_PER_BLOCK_t - 1) / SLOTS_PER_BLOCK_t;
    uint64_t blocksize=17;

    return ((nblocks)*(blocksize+8*(slotSize+fcounter+value_bits)))/1024;

}

void kDataFrameBMQF::estimateParameters(vector<uint64_t> countHistogram,
                                        uint64_t numHashBits,uint64_t value_bits,
                                        uint64_t *res_noSlots,uint64_t *res_fixedSizeCounter, uint64_t *res_memory){

    uint64_t noDistinctKmers=countHistogram[0];
    *res_memory=numeric_limits<uint64_t>::max();
    for(int i=8;i<64;i++)
    {
        uint64_t noSlots=(1ULL)<<i;
        if(noSlots<noDistinctKmers)
            continue;
        bool moreWork=false;
        uint64_t slotSize=numHashBits-log2((double)noSlots);
        for(uint64_t fixedSizeCounter=1;fixedSizeCounter<slotSize;fixedSizeCounter++)
        {
            if(isEnough(countHistogram,noSlots,fixedSizeCounter,slotSize))
            {
                uint64_t tmpMem=estimateMemory(noSlots,slotSize,fixedSizeCounter,value_bits);
                if(*res_memory>tmpMem)
                {
                    *res_memory=tmpMem;
                    *res_fixedSizeCounter=fixedSizeCounter;
                    *res_noSlots=noSlots;
                    moreWork=true;
                }
                else{
                    break;
                }
            }

        }
        if(!moreWork && *res_memory!=numeric_limits<uint64_t>::max())
            break;
    }
    if(*res_memory==numeric_limits<uint64_t>::max())
    {
        throw std::overflow_error("Data limits exceeds MQF capabilities(> uint64). Check if ntcard file is corrupted");
    }

}

bool kDataFrameBMQF::isEnough(vector<uint64_t> histogram,uint64_t noSlots,uint64_t fixedSizeCounter,uint64_t slotSize)
{
    // cout<<"noSlots= "<<noSlots<<endl
    //     <<"fcounter= "<<fixedSizeCounter<<endl
    //     <<"slot size= "<<numHashBits<<endl;

    noSlots=(uint64_t)((double)noSlots*0.90);
    for(uint64_t i=1;i<1000;i++)
    {
        uint64_t usedSlots=1;

        if(i>((1ULL)<<fixedSizeCounter)-1)
        {
            uint64_t nSlots2=0;
            __uint128_t capacity;
            do{
                nSlots2++;
                capacity=((__uint128_t)(1ULL)<<(nSlots2*slotSize+fixedSizeCounter))-1;
                //  cout<<"slots num "<<nSlots2<<" "<<capacity<<endl;
            }while((__uint128_t)i>capacity);
            usedSlots+=nSlots2;
        }
        //cout<<"i= "<<i<<"->"<<usedSlots<<" * "<<histogram[i]<<endl;
        if(noSlots>=(usedSlots*histogram[i]))
        {
            noSlots-=(usedSlots*histogram[i]);
        }
        else
        {
            //  cout<<"failed"<<endl<<endl;
            return false;
        }

    }
    //cout<<"success"<<endl<<endl;
    return true;
}

bool kDataFrameBMQF::setCount(string kmer,uint64_t count){
    uint64_t hash=KD->hash_kmer(kmer)% bufferedmqf->memoryBuffer->metadata->range;
    uint64_t currentCount=bufferedMQF_count_key(bufferedmqf,hash);
    if(currentCount>count){
        bufferedMQF_remove(bufferedmqf,hash,currentCount-count,false,false);
    }
    else{
        try{
            bufferedMQF_insert(bufferedmqf,hash,count-currentCount,false,false);
        }
        catch(overflow_error & e)
        {
            reserve(bufferedmqf->memoryBuffer->metadata->nslots);
            return setCount(kmer,count);
        }
    }
    return true;
}

bool kDataFrameBMQF::insert(string kmer,uint64_t count){
    uint64_t hash=KD->hash_kmer(kmer) % bufferedmqf->memoryBuffer->metadata->range;
    try{
        bufferedMQF_insert(bufferedmqf,hash,count,true,true);
    }
    catch(overflow_error & e)
    {
        reserve(bufferedmqf->disk->metadata->nslots);
        return insert(kmer,count);
    }
    return true;
}
bool kDataFrameBMQF::insert(string kmer){
    if(load_factor()>0.9){
        // ERROR FLAG: reserve(bufferedmqf->memoryBuffer->metadata->nslots)
        reserve(bufferedmqf->memoryBuffer->metadata->nslots);
    }
    uint64_t hash= KD->hash_kmer(kmer) % bufferedmqf->memoryBuffer->metadata->range;
    try{
        bufferedMQF_insert(bufferedmqf,hash,1,false,false);
    }
    catch(overflow_error & e)
    {
        reserve(bufferedmqf->disk->metadata->nslots);
        return insert(kmer);
    }
    return true;
}

uint64_t kDataFrameBMQF::getCount(string kmer){
    uint64_t hash=KD->hash_kmer(kmer) % bufferedmqf->memoryBuffer->metadata->range;
    return bufferedMQF_count_key(bufferedmqf,hash);
}



bool kDataFrameBMQF::erase(string kmer){
    uint64_t hash=KD->hash_kmer(kmer)% bufferedmqf->memoryBuffer->metadata->range;
    uint64_t currentCount=bufferedMQF_count_key(bufferedmqf,hash);

    bufferedMQF_remove(bufferedmqf,hash,currentCount,false,false);
    return true;
}

bool kDataFrameBMQF::setCount(uint64_t  hash,uint64_t count){
    uint64_t currentCount=bufferedMQF_count_key(bufferedmqf,hash);
    if(currentCount>count){
        bufferedMQF_remove(bufferedmqf,hash,currentCount-count,false,false);
    }
    else{
        try{
            bufferedMQF_insert(bufferedmqf,hash,count-currentCount,false,false);
        }
        catch(overflow_error & e)
        {
            reserve(bufferedmqf->memoryBuffer->metadata->nslots);
            return setCount(hash,count);
        }
    }
    return true;
}

bool kDataFrameBMQF::insert(uint64_t hash,uint64_t count){
    try{
        bufferedMQF_insert(bufferedmqf,hash,count,true,true);
    }
    catch(overflow_error & e)
    {
        reserve(bufferedmqf->disk->metadata->nslots);
        return insert(hash,count);
    }
    return true;
}
bool kDataFrameBMQF::insert(uint64_t hash){
    if(load_factor()>0.9){
        // ERROR FLAG: reserve(bufferedmqf->memoryBuffer->metadata->nslots)
        reserve(bufferedmqf->memoryBuffer->metadata->nslots);
    }
    try{
        bufferedMQF_insert(bufferedmqf,hash,1,false,false);
    }
    catch(overflow_error & e)
    {
        reserve(bufferedmqf->disk->metadata->nslots);
        return insert(hash);
    }
    return true;
}

uint64_t kDataFrameBMQF::getCount(uint64_t hash){
    return bufferedMQF_count_key(bufferedmqf,hash);
}



bool kDataFrameBMQF::erase(uint64_t hash){
    uint64_t currentCount=bufferedMQF_count_key(bufferedmqf,hash);

    bufferedMQF_remove(bufferedmqf,hash,currentCount,false,false);
    return true;
}

uint64_t kDataFrameBMQF::size(){
    return bufferedmqf->disk->metadata->ndistinct_elts+bufferedmqf->memoryBuffer->metadata->ndistinct_elts;
}
uint64_t kDataFrameBMQF::max_size(){
    return bufferedmqf->disk->metadata->xnslots;
}
float kDataFrameBMQF::load_factor(){
    return (float)bufferedMQF_space(bufferedmqf)/100.0;
}
float kDataFrameBMQF::max_load_factor(){
    return 0.9;
}


void kDataFrameBMQF::save(string filePath){
    throw logic_error("Not Implemented yet!");
    //filePath += ".mqf";
    ofstream file(filePath+".extra");
    file<<kSize<<endl;
    // uint64_t legendSize=tagsLegend.size();
    // file<<legendSize<<endl;
    // auto it = tagsLegend.begin();
    // while(it==tagsLegend.end())
    // {
    //   file<<it->first<<" "<<it->second<<endl;
    //   it++;
    // }
    // file.close();
    qf_serialize(bufferedmqf->memoryBuffer,(filePath+".mqf").c_str());
}
kDataFrame* kDataFrameBMQF::load(string filePath){
    throw logic_error("Not Implemented yet!");
    ifstream file(filePath+".extra");
    uint64_t filekSize;
    file>>filekSize;
    bufferedMQF* bufferedmqf=new bufferedMQF();
    qf_deserialize(bufferedmqf->memoryBuffer,(filePath+".mqf").c_str());
    return new kDataFrameBMQF(bufferedmqf,filekSize,0);
}

kDataFrameIterator kDataFrameBMQF::begin(){
    //bufferedMQF_syncBuffer(bufferedmqf);
    return (kDataFrameIterator(
            (_kDataFrameIterator*)new kDataFrameBMQFIterator(bufferedmqf,kSize,KD),
            (kDataFrame*)this));
}

kDataFrameIterator kDataFrameBMQF::end(){
    kDataFrameBMQFIterator* it=new kDataFrameBMQFIterator(bufferedmqf,kSize,KD);
    it->endIterator();
    return (kDataFrameIterator(it,(kDataFrame*)this));
}

kDataFrameIterator kDataFrameBMQF::find(string kmer) {
    bufferedMQFIterator* mqfIt = new bufferedMQFIterator();
    uint64_t hash=KD->hash_kmer(kmer)% bufferedmqf->memoryBuffer->metadata->range;
    bufferedMQF_find(bufferedmqf,mqfIt,hash);
    kDataFrameBMQFIterator* it=new kDataFrameBMQFIterator(bufferedmqf,mqfIt,kSize,KD);
    return (kDataFrameIterator(it, (kDataFrame *) this));
}

/*
 *****************************
 *** kDataFrameBMQFIterator ***
 *****************************
 */


kDataFrameBMQFIterator::kDataFrameBMQFIterator(bufferedMQF *mqf, uint64_t kSize, kmerDecoder* KD)
        : _kDataFrameIterator(kSize) {
    this->mqf=mqf;
    qfi=bufferedMQF_iterator(mqf, 0);
    this->KD =KD;
}
kDataFrameBMQFIterator::kDataFrameBMQFIterator(bufferedMQF* mqf,bufferedMQFIterator *It, uint64_t kSize, kmerDecoder *KD)
        : _kDataFrameIterator(kSize) {
    qfi = It;
    this->KD = KD;
    this->mqf=mqf;
}
kDataFrameBMQFIterator::kDataFrameBMQFIterator(const kDataFrameBMQFIterator &other) :
        _kDataFrameIterator(other.kSize) {
    qfi = new bufferedMQFIterator();
    mqf=other.mqf;
    qfi->bufferIt=new QFi();
    qfi->bufferIt->qf = other.qfi->bufferIt->qf;
    qfi->bufferIt->run = other.qfi->bufferIt->run;
    qfi->bufferIt->current = other.qfi->bufferIt->current;
    qfi->bufferIt->cur_start_index = other.qfi->bufferIt->cur_start_index;
    qfi->bufferIt->cur_length = other.qfi->bufferIt->cur_length;
    qfi->bufferIt->num_clusters = other.qfi->bufferIt->num_clusters;
    qfi->bufferIt->c_info = other.qfi->bufferIt->c_info;

    qfi->diskIt=new onDiskMQF_Namespace::onDiskMQFIterator();
    qfi->diskIt->qf = other.qfi->diskIt->qf;
    qfi->diskIt->run = other.qfi->diskIt->run;
    qfi->diskIt->current = other.qfi->diskIt->current;
    qfi->diskIt->cur_start_index = other.qfi->diskIt->cur_start_index;
    qfi->diskIt->cur_length = other.qfi->diskIt->cur_length;
    qfi->diskIt->num_clusters = other.qfi->diskIt->num_clusters;
    qfi->diskIt->c_info = other.qfi->diskIt->c_info;

    KD = other.KD;
}

_kDataFrameIterator *kDataFrameBMQFIterator::clone() {
    return new kDataFrameBMQFIterator(*this);
}


kDataFrameBMQFIterator &kDataFrameBMQFIterator::operator++(int) {
    qfi->next();
    return *this;
}

uint64_t kDataFrameBMQFIterator::getHashedKmer() {
    uint64_t key, value, count;
    qfi->get( &key, &value, &count);
    return key;

}

string kDataFrameBMQFIterator::getKmer() {
    return KD->ihash_kmer(getHashedKmer());
}

uint64_t kDataFrameBMQFIterator::getCount() {
    uint64_t key, value, count;
    qfi->get(&key, &value, &count);
    return count;
}

bool kDataFrameBMQFIterator::setCount(uint64_t count) {
    uint64_t key, value, currentCount;
    qfi->get( &key, &value, &currentCount);
    if (currentCount > count) {
        bufferedMQF_remove(mqf, key, currentCount - count, false, false);
    } else {
        bufferedMQF_insert(mqf, key, count - currentCount, false, false);
    }
    return true;
}

void kDataFrameBMQFIterator::endIterator() {
    qfi->bufferIt->current = qfi->bufferIt->qf->metadata->xnslots;
    qfi->diskIt->current = qfi->diskIt->qf->metadata->xnslots;
    qfi->finished=true;
}

bool kDataFrameBMQFIterator::operator==(const _kDataFrameIterator &other) {
    if(qfi->end() && ((kDataFrameBMQFIterator *) &other)->qfi->end())
        return true;
    else if(qfi->end() || ((kDataFrameBMQFIterator *) &other)->qfi->end())
        return false;
    return getHashedKmer() == ((kDataFrameBMQFIterator *) &other)->getHashedKmer();

}


bool kDataFrameBMQFIterator::operator!=(const _kDataFrameIterator &other) {
    if(qfi->end() && ((kDataFrameBMQFIterator *) &other)->qfi->end())
        return false;
    else if(qfi->end() || ((kDataFrameBMQFIterator *) &other)->qfi->end())
        return true;
    return getHashedKmer() != ((kDataFrameBMQFIterator *) &other)->getHashedKmer();
}

kDataFrameBMQFIterator::~kDataFrameBMQFIterator() {
    delete qfi;
}
