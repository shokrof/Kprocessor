class kDataFrameMQF: public kDataFrame{
    public:
        kDataFrameMQF();
        kDataFrameMQF(std::uint64_t kSize);
        kDataFrameMQF(std::uint64_t kSize, int mode);
        kDataFrameMQF(std::uint64_t ksize,uint8_t q,uint8_t fixedCounterSize,uint8_t tagSize ,double falsePositiveRate);

        kDataFrameMQF(std::uint64_t ksize, uint8_t q, int mode);

//        kDataFrameMQF(QF* mqf,std::uint64_t ksize,double falsePositiveRate);
        //count histogram is array where count of kmers repeated n times is found at index n. index 0 holds number of distinct kmers.
//        kDataFrameMQF(std::uint64_t ksize,vector<std::uint64_t> countHistogram,uint8_t tagSize ,double falsePositiveRate);
//        kDataFrameMQF(std::uint64_t kSize,vector<std::uint64_t> kmersHistogram);

        ~kDataFrameMQF(){
            qf_destroy(mqf);
            delete mqf;
        }
        void reserve (std::uint64_t n);
//        void reserve (vector<std::uint64_t> countHistogram);
        kDataFrame* getTwin();

//        static std::uint64_t estimateMemory(std::uint64_t nslots,std::uint64_t slotSize, std::uint64_t fcounter, std::uint64_t tagSize);


//        static void estimateParameters(vector<std::uint64_t> countHistogram, std::uint64_t numHashBits,std::uint64_t tagSize, std::uint64_t *res_noSlots,std::uint64_t *res_fixedSizeCounter, std::uint64_t *res_memory);


        bool setCount(string kmer,std::uint64_t count);
        bool setCount(std::uint64_t kmer, std::uint64_t count);
        bool insert(string kmer,std::uint64_t count);
        bool insert(string kmer);
        bool insert(std::uint64_t kmer, std::uint64_t count);
        bool insert(std::uint64_t kmer);
        std::uint64_t getCount(string kmer);
        std::uint64_t getCount(std::uint64_t kmer);


        bool erase(string kmer);
        bool erase(std::uint64_t kmer);

        std::uint64_t size();
/// max_size function returns the estimated maximum number of kmers that the kDataframeMQF can hold.
/*! The number of kmers is estimated as if all the kmers repeated 2^(fixed counter size)-1 times.*/
        std::uint64_t max_size();
        float load_factor();
        float max_load_factor();


//        QF* getMQF(){
//            return mqf;
//        }

        void serialize(string filePath);
        static kDataFrame* load(string filePath);

        kDataFrameIterator begin();
        // kDataFrameIterator end();
        kDataFrameIterator find(string kmer);
        kDataFrameIterator find(uint64_t kmer);

        bool kmerExist(string kmerS);
};
