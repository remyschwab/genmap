#include <vector>
#include <string>
#include <cstdint>

// TODO: investigate performance of buffer sizes (stack overflow might occur leading to a segmentation fault)
#define     BUFFER_SIZE     32*1024 // 32 KB

using namespace seqan;

template <typename T>
void saveRaw(std::vector<T> const & c, std::string const & output_path, bool const mappability)
{
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(output_path, std::ios::out | std::ios::binary);
    outfile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    if (mappability)
    {
        for (T const v : c)
        {
            float const f = (v != 0) ? 1.0f / static_cast<float>(v) : 0;
            outfile.write(reinterpret_cast<const char*>(&f), sizeof(float));
        }
    }
    else
    {
        outfile.write((const char*) &c[0], c.size() * sizeof(T));
    }

    outfile.close();
}

template <typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveTxt(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths, bool const mappability)
{
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(output_path + ".txt", std::ios::out | std::ofstream::binary);
    outfile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    auto seqBegin = c.begin();
    auto seqEnd = c.begin() + chromLengths[0];
    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        outfile << '>' << chromNames[i] << '\n';

        if (mappability)
        {
            for (auto it = seqBegin; it < seqEnd - 1; ++it)
            {
                float const f = (*it != 0) ? 1.0f / static_cast<float>(*it) : 0;
                outfile << f << ' ';
            }
            float const f = (*(seqEnd - 1) != 0) ? 1.0f / static_cast<float>(*(seqEnd - 1)) : 0;
            outfile << f; // no space after last value
        }
        else
        {
            std::copy(seqBegin, seqEnd - 1, std::ostream_iterator<uint16_t>(outfile, " "));
            outfile << static_cast<uint16_t>(*(seqEnd - 1)); // no space after last value
        }
        outfile << '\n';

        if (i + 1 < length(chromLengths))
        {
            seqBegin = seqEnd;
            seqEnd += chromLengths[i + 1];
        }
    }
    outfile.close();
}

// TODO: do not output sequences in .chrom.sizes if no entries are written to .wig
template <typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveWig(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths, bool const mappability)
{
    uint64_t pos = 0;
    uint64_t begin_pos_string = 0;
    uint64_t end_pos_string = chromLengths[0];

    char buffer[BUFFER_SIZE];

    std::ofstream wigFile(output_path + ".wig");
    wigFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        uint16_t current_val = c[pos];
        uint64_t occ = 0;
        uint64_t last_occ = 0;

        while (pos < end_pos_string + 1) // iterate once more to output the last line
        {
            if (pos == end_pos_string || current_val != c[pos])
            {
                // TODO: document this behavior (mappability of 0)
                if (current_val != 0)
                {
                    if (last_occ != occ)
                        wigFile << "variableStep chrom=" << chromNames[i] << " span=" << occ << '\n';

                    if (mappability)
                    {
                        float const value = (current_val != 0) ? 1.0f / static_cast<float>(current_val) : 0;
                        wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << value << '\n'; // pos in wig start at 1
                    }
                    else
                    {
                        wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << current_val << '\n'; // pos in wig start at 1
                    }
                    last_occ = occ;
                }

                occ = 0;
                if (pos < end_pos_string)
                    current_val = c[pos];
            }

            ++occ;
            ++pos;
        }
        --pos; // pos is incremented once too often by the additional last iteration, i.e., pos == end_pos_string

        begin_pos_string += chromLengths[i];
        if (i + 1 < length(chromLengths))
            end_pos_string += chromLengths[i + 1];
    }
    wigFile.close();

    // .chrom.sizes file
    std::ofstream chromSizesFile(output_path + ".chrom.sizes");
    for (uint64_t i = 0; i < length(chromLengths); ++i)
        chromSizesFile << chromNames[i] << '\t' << chromLengths[i] << '\n';
    chromSizesFile.close();
}

template <typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveBedGraph(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths, bool const bedGraphFormat, bool const mappability)
{
    uint64_t pos = 0;
    uint64_t begin_pos_string = 0;
    uint64_t end_pos_string = chromLengths[0];

    char buffer[BUFFER_SIZE];

    std::ofstream bedgraphFile(output_path + (bedGraphFormat ? ".bedgraph" : ".bed"));
    bedgraphFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        uint16_t current_val = c[pos];
        uint64_t occ = 0;

        while (pos < end_pos_string + 1) // iterate once more to output the last line
        {
            if (pos == end_pos_string || current_val != c[pos])
            {
                if (current_val != 0)
                {
                    bedgraphFile << chromNames[i] << '\t'                    // chrom name
                                 << (pos - occ - begin_pos_string) << '\t'   // start pos (begins with 0)
                                 << (pos - begin_pos_string) << '\t';        // name

                    if (!bedGraphFormat)
                        bedgraphFile << '-' << '\t';

                    if (mappability)
                        bedgraphFile << ((current_val != 0) ? 1.0f / static_cast<float>(current_val) : 0) << '\n';
                    else
                        bedgraphFile << current_val << '\n';
                }

                occ = 0;
                if (pos < end_pos_string)
                    current_val = c[pos];
            }

            ++occ;
            ++pos;
        }
        --pos; // pos is incremented once too often by the additional last iteration, i.e., pos == end_pos_string

        begin_pos_string += chromLengths[i];
        if (i + 1 < length(chromLengths))
            end_pos_string += chromLengths[i + 1];
    }
    bedgraphFile.close();
}

template <typename TLocations, typename TDirectoryInformation, typename TCSVIntervals>
void saveCsv(std::string const & output_path, TLocations const & locations,
             SearchParams const & searchParams, TDirectoryInformation const & directoryInformation,
             TCSVIntervals const & csvIntervals, bool const outputSelection)
{
    char buffer[BUFFER_SIZE];

    std::ofstream csvFile(output_path + ".csv");
    csvFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    uint64_t chromosomeCount = 0;
    std::vector<std::pair<std::string, uint64_t> > fastaFiles; // fasta file, cumulative nbr. of chromosomes
    std::string lastFastaFile = std::get<0>(retrieveDirectoryInformationLine(directoryInformation[0]));
    for (auto const & row : directoryInformation)
    {
        auto const line = retrieveDirectoryInformationLine(row);
        if (lastFastaFile != std::get<0>(line))
        {
            fastaFiles.push_back({lastFastaFile, chromosomeCount - 1});
            lastFastaFile = std::get<0>(line);
        }
        ++chromosomeCount;
    }

    csvFile << "\"k-mer\"";
    for (auto const & fastaFile : fastaFiles)
        csvFile << ";\"+ strand " << fastaFile.first << "\"";
    if (searchParams.revCompl)
    {
        for (auto const & fastaFile : fastaFiles)
            csvFile << ";\"- strand " << fastaFile.first << "\"";
    }
    csvFile << '\n';

    auto interval = csvIntervals.begin();

    for (auto const & kmerLocations : locations)
    {
        auto const & kmerPos = kmerLocations.first;
        auto const & plusStrandLoc = kmerLocations.second.first;
        auto const & minusStrandLoc = kmerLocations.second.second;

        while (interval != csvIntervals.end() &&
               ((std::get<0>(*interval) < kmerPos.i1) ||
                (std::get<0>(*interval) == kmerPos.i1 && std::get<2>(*interval) <= kmerPos.i2)))
        {
            ++interval;
        }

        if (outputSelection &&
            !(std::get<0>(*interval) == kmerPos.i1 &&
              std::get<1>(*interval) <= kmerPos.i2 &&
              kmerPos.i2 < std::get<2>(*interval)))
        {
            continue;
        }

        csvFile << kmerPos.i1 << ',' << kmerPos.i2;

        uint64_t i = 0;
        uint64_t nbrChromosomesInPreviousFastas = 0;
        for (auto const & fastaFile : fastaFiles)
        {
            csvFile << ';';
            bool subsequentIterations = false;
            while (i < plusStrandLoc.size() && plusStrandLoc[i].i1 <= fastaFile.second)
            {
                if (subsequentIterations)
                    csvFile << '|'; // separator for multiple locations in one column
                csvFile << (plusStrandLoc[i].i1 - nbrChromosomesInPreviousFastas) << ',' << plusStrandLoc[i].i2;
                subsequentIterations = true;
                ++i;
            }
            nbrChromosomesInPreviousFastas = fastaFile.second + 1;
        }

        if (searchParams.revCompl)
        {
            uint64_t i = 0;
            uint64_t nbrChromosomesInPreviousFastas = 0;
            for (auto const & fastaFile : fastaFiles)
            {
                csvFile << ';';
                bool subsequentIterations = false;
                while (i < minusStrandLoc.size() && minusStrandLoc[i].i1 <= fastaFile.second)
                {
                    if (subsequentIterations)
                        csvFile << '|'; // separator for multiple locations in one column
                    csvFile << (minusStrandLoc[i].i1 - nbrChromosomesInPreviousFastas) << ',' << minusStrandLoc[i].i2;
                    subsequentIterations = true;
                    ++i;
                }
                nbrChromosomesInPreviousFastas = fastaFile.second + 1;
            }
        }
        csvFile << '\n';
    }

    csvFile.close();
}

template <bool mappability, typename T, typename TLocations, typename TDirectoryInformation, typename TCSVIntervals, typename TText, typename TChromosomeLengths>
void saveDesignFile(std::vector<T> const & c, std::string const & /*output_path*/, TLocations const & locations,
                    SearchParams const & searchParams, TDirectoryInformation const & directoryInformation,
                    TCSVIntervals const & /*svIntervals*/, bool const /*outputSelection*/, DesignFileOutput & designFileOutput,
                    uint64_t const currentFileNo, Options const & opt, TText const & text, TChromosomeLengths const & chromCumLengths)
{
    //uint64_t const nbr_of_genomes = designFileOutput.matrix.size();

    // extract fasta file names
    uint64_t chromosomeCount = 0;
    std::vector<std::pair<std::string, uint64_t> > fastaFiles; // fasta file, cumulative nbr. of chromosomes
    std::string lastFastaFile = std::get<0>(retrieveDirectoryInformationLine(directoryInformation[0]));
    for (auto const & row : directoryInformation)
    {
        auto const line = retrieveDirectoryInformationLine(row);
        if (lastFastaFile != std::get<0>(line))
        {
            fastaFiles.push_back({lastFastaFile, chromosomeCount - 1});
            lastFastaFile = std::get<0>(line);
        }
        ++chromosomeCount;
    }

//    std::map<Pair<TSeqNo, TSeqPos>,
//    std::pair<std::vector<Pair<TSeqNo, TSeqPos> >,
//    std::vector<Pair<TSeqNo, TSeqPos> > > > locations;
    auto location_it = locations.begin();

    uint64_t window_size = 1 * searchParams.length / opt.design_sample_rate; // and then pick the 10 rarest k-mers, non-overlapping

    std::vector<uint64_t> pos_in_window;
    std::vector<uint64_t> all_min_pos;

    for (uint64_t i = 0; i < c.size(); i += pos_in_window.size())
    {
        all_min_pos.clear();

        pos_in_window.clear();
        pos_in_window.resize(std::min(window_size, c.size() - i));
        std::iota(pos_in_window.begin(), pos_in_window.end(), i); // TODO: maybe shuffle afterwards for some randomization?
        // sort positions by the number of times their k-mer occurs
        std::sort(pos_in_window.begin(), pos_in_window.end(), [&c](uint64_t p1, uint64_t p2){ return c[p1] < c[p2]; });

        // extract 10-rarest k-mers that do not overlap with each other
        for (uint64_t idx = 0; idx < pos_in_window.size() && all_min_pos.size() < 1; ++idx)
        {
            const uint64_t rare_kmer_location = pos_in_window[idx];

            if (c[rare_kmer_location] == 0)
                continue; // last K-1 positions in a sequence (is not in 'locations')

            // check that there is no overlap
            if (std::find_if(all_min_pos.begin(), all_min_pos.end(), [rare_kmer_location](uint64_t loc){
                    return rare_kmer_location - (30 - 1) <= loc && loc <= rare_kmer_location + 30 - 1; // this is an overlap
                }) == all_min_pos.end())
                all_min_pos.push_back(rare_kmer_location);
        }

        std::sort(all_min_pos.begin(), all_min_pos.end()); // we need to sort this so we don't access an earlier position
        // this allows us to speed up the search in 'locations'

        for (const uint64_t current_min_pos : all_min_pos)
        {
            // transform min_pos to tuple
            Pair<uint64_t, uint64_t> min_pos_tuple;
            myPosLocalize(min_pos_tuple, current_min_pos, chromCumLengths);

            // extract element from 'location'
            auto prev_location_it = location_it;
            location_it = std::find_if(location_it, locations.end(), [&min_pos_tuple](auto const & l){
                return l.first.i1 == min_pos_tuple.i1 && l.first.i2 == min_pos_tuple.i2;
            });

            if (location_it == locations.end())
            {
                std::cout << "prev_location_it: " << (*prev_location_it).first << '\n';

                std::cout << "NOT FOUND: (" << min_pos_tuple.i1 << ',' << min_pos_tuple.i2 << "): " << current_min_pos << "(min_value: " << (unsigned) c[current_min_pos] << ")\n";

                uint64_t prev = 0;
                for (auto x : chromCumLengths)
                {
                    std::cout << x-prev << '\t' << x << std::endl;
                    prev = x;
                }

                std::cout << "Locations: \n";
                for (auto const & kmerLocations : locations)
                {
                    auto const &kmerPos = kmerLocations.first;
                    if (kmerPos.i1 == 0)
                        std::cout << kmerPos.i1 << ',' << kmerPos.i2 << std::endl;
                }
                exit(23);
            }

            // extract kmer at position and make it canonical
            Dna5String kmer = infixWithLength(text, current_min_pos, searchParams.length);
            Dna5String kmer_rc = kmer;
            reverseComplement(kmer_rc);
            if (kmer > kmer_rc)
                kmer = kmer_rc;
            //std::cout << min_pos << '\t' << '(' << min_pos_tuple.i1 << ',' << min_pos_tuple.i2 << ")\t" << kmer << '\n';

            // extract kmer_id / store kmer in std::map
            auto lb = designFileOutput.kmer_id.lower_bound(kmer);

            uint64_t kmer_id = 0;
            if(lb != designFileOutput.kmer_id.end() && !(designFileOutput.kmer_id.key_comp()(kmer, lb->first)))
            {
                // key already exists
                kmer_id = lb->second;
            }
            else
            {
                // the key does not exist in the map
                // add it to the map
                kmer_id = designFileOutput.kmer_id.size() + 1;
                designFileOutput.kmer_id.insert(lb, {kmer, kmer_id});
            }

            // add probe to matrix (currentFileNo starts with 1)
            designFileOutput.matrix[currentFileNo - 1].insert(kmer_id);

            // add matches to other genomes in matrix
            auto location = *location_it;

            auto const & plusStrandLoc = location.second.first;
            uint64_t fastaID = 0;
            uint64_t m = 0;
            for (auto const & fastaFile : fastaFiles)
            {
                bool kmerInFasta = false;
                while (m < plusStrandLoc.size() && plusStrandLoc[m].i1 <= fastaFile.second)
                {
                    kmerInFasta = true;
                    ++m;
                }
                if (kmerInFasta)
                {
                    designFileOutput.matrix[fastaID].insert(kmer_id);
                }

                ++fastaID;
            }

            if (searchParams.revCompl)
            {
                auto const & minusStrandLoc = location.second.second;
                fastaID = 0;
                m = 0;
                for (auto const & fastaFile : fastaFiles)
                {
                    bool kmerInFasta = false;
                    while (m < minusStrandLoc.size() && minusStrandLoc[m].i1 <= fastaFile.second)
                    {
                        kmerInFasta = true;
                        ++m;
                    }
                    if (kmerInFasta)
                    {
                        designFileOutput.matrix[fastaID].insert(kmer_id);
                    }

                    ++fastaID;
                }
            }
        }
    }
}
