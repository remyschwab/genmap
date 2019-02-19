#include <vector>
#include <string>
#include <cstdint>

// TODO: investigate performance of buffer sizes!
// TODO: stack overflow might occur leading to a segmentation fault!
#define     BUFFER_SIZE     32*1024 // 32 KB

using namespace seqan;

inline std::string get_output_path(Options const & opt, SearchParams const & /*searchParams*/, std::string const & fastaFile)
{
    std::string path = std::string(toCString(opt.outputPath));
    if (back(path) != '/')
        path += '/';
    path += fastaFile.substr(0, fastaFile.find_last_of('.'));
    path += ".genmap";
    return path;
}

// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
void saveRawFreq(std::vector<T> const & c, std::string const & output_path)
{
    std::ofstream outfile(output_path, std::ios::out | std::ios::binary);
    outfile.write((const char*) &c[0], c.size() * sizeof(T));
    outfile.close();
}

template <typename T>
void saveRawMap(std::vector<T> const & c, std::string const & output_path)
{
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(output_path);
    outfile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (T const & v : c)
    {
        float const f = (v != 0) ? 1.0 / static_cast<float>(v) : 0;
        outfile.write(reinterpret_cast<const char*>(&f), sizeof(float));
    }
    outfile.close();
}

// ---------------------------------------------------------------------------------------------------------------------

template <bool mappability, typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveTxt(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths)
{
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(output_path + ".txt", std::ios::out | std::ofstream::binary);
    outfile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    auto seqBegin = c.begin();
    auto seqEnd = c.begin() + chromLengths[0];
    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        outfile << '>' << chromNames[i] << '\n';
        // TODO: remove space at end of line
        SEQAN_IF_CONSTEXPR (mappability)
        {
            for (auto it = seqBegin; it < seqEnd; ++it)
            {
                float const f = (*it != 0) ? 1.0 / static_cast<float>(*it) : 0;
                // outfile.write(reinterpret_cast<const char*>(&f), sizeof(float));
                outfile << f << ' ';
            }
        }
        else
        {
            std::copy(seqBegin, seqEnd, std::ostream_iterator<T>(outfile, " "));
            // std::copy(seqBegin, seqEnd, (std::ostream_iterator<T>(outfile), std::ostream_iterator<int>(outfile, " ")));
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

// ---------------------------------------------------------------------------------------------------------------------

template <bool mappability, typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveWig(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths)
{
    uint64_t pos = 0;
    uint64_t begin_pos_string = 0;
    uint64_t end_pos_string = std::min<uint64_t>(chromLengths[0], c.size());

    char buffer[BUFFER_SIZE];

    std::ofstream wigFile(output_path + ".wig");
    wigFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        uint16_t current_val = c[pos];
        uint64_t occ = 0;
        uint64_t last_occ = 0;

        while (pos < end_pos_string)
        {
            if (current_val != c[pos])
            {
                if (last_occ != occ)
                    wigFile << "variableStep chrom=" << chromNames[i] << " span=" << occ << '\n';
                // TODO: document this behavior (mappability of 0)
                SEQAN_IF_CONSTEXPR (mappability)
                {
                    float const value = (current_val != 0) ? 1.0 / static_cast<float>(current_val) : 0;
                    wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << value << '\n'; // pos in wig start at 1
                }
                else
                {
                    wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << current_val << '\n'; // pos in wig start at 1
                }

                last_occ = occ;
                occ = 0;
                current_val = c[pos];
            }

            ++occ;
            ++pos;
        }

        // TODO: remove this block by appending a different value to c (reserve one more. check performance)
        if (last_occ != occ)
            wigFile << "variableStep chrom=" << chromNames[i] << " span=" << occ << '\n';
        SEQAN_IF_CONSTEXPR (mappability)
        {
            float const value = (current_val != 0) ? 1.0 / static_cast<float>(current_val) : 0;
            wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << value << '\n'; // pos in wig start at 1
        }
        else
        {
            wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << current_val << '\n'; // pos in wig start at 1
        }

        begin_pos_string += chromLengths[i];
        if (i + 1 < length(chromLengths))
            end_pos_string += chromLengths[i + 1];
        end_pos_string = std::min<uint64_t>(end_pos_string, c.size()); // last chromosomeLength has to be reduced by K-1 characters
    }
    wigFile.close();

    // .chrom.sizes file
    std::ofstream chromSizesFile(output_path + ".chrom.sizes");
    for (uint64_t i = 0; i < length(chromLengths); ++i)
        chromSizesFile << chromNames[i] << '\t' << chromLengths[i] << '\n';
    chromSizesFile.close();

    // std::cout << "Wig file stored!" << std::endl;
}

// ---------------------------------------------------------------------------------------------------------------------

template <bool mappability, typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveBed(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths)
{
    uint64_t pos = 0;
    uint64_t begin_pos_string = 0;
    uint64_t end_pos_string = std::min<uint64_t>(chromLengths[0], c.size());

    char buffer[BUFFER_SIZE];

    std::ofstream bedFile(output_path + ".bed");
    bedFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        uint16_t current_val = c[pos];
        uint64_t occ = 0;

        while (pos < end_pos_string)
        {
            if (current_val != c[pos])
            {
                bedFile << chromNames[i] << '\t'                    // chrom name
                        << (pos - occ - begin_pos_string) << '\t'   // start pos (begins with 0)
                        << (pos - begin_pos_string - 1) << '\t'     // end pos
                        << '-' << '\t';                             // name

                SEQAN_IF_CONSTEXPR (mappability)
                    bedFile << ((current_val != 0) ? 1.0 / static_cast<float>(current_val) : 0) << '\n';
                else
                    bedFile << current_val << '\n';

                occ = 0;
                current_val = c[pos];
            }

            ++occ;
            ++pos;
        }

        // TODO: remove this block by appending a different value to c (reserve one more. check performance)
        bedFile << chromNames[i] << '\t'                    // chrom name
                << (pos - occ - begin_pos_string) << '\t'   // start pos (begins with 0)
                << (pos - begin_pos_string - 1) << '\t'     // end pos
                << '-' << '\t';                             // name

        SEQAN_IF_CONSTEXPR (mappability)
            bedFile << ((current_val != 0) ? 1.0 / static_cast<float>(current_val) : 0) << '\n';
        else
            bedFile << current_val << '\n';

        begin_pos_string += chromLengths[i];
        if (i + 1 < length(chromLengths))
            end_pos_string += chromLengths[i + 1];
        end_pos_string = std::min<uint64_t>(end_pos_string, c.size()); // last chromosomeLength has to be reduced by K-1 characters
    }
    bedFile.close();
}

// ---------------------------------------------------------------------------------------------------------------------

template <bool mappability, typename T, typename TLocations, typename TDirectoryInformation>
void saveCsv(std::vector<T> const & /*c*/, std::string const & output_path, TLocations const & locations,
             Options const & /*opt*/, SearchParams const & searchParams, TDirectoryInformation const & directoryInformation)
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

    // TODO: for each filename:
    csvFile << "\"k-mer\"";
    for (auto const & fastaFile : fastaFiles)
    {
        csvFile << ";\"+ strand " << fastaFile.first << "\"";
        if (searchParams.revCompl) // TODO: make it constexpr?
            csvFile << ";\"- strand " << fastaFile.first << "\"";
    }
    csvFile << '\n';

    for (auto const & kmerLocations : locations)
    {
        auto const & kmerPos = kmerLocations.first;
        auto const & plusStrandLoc = kmerLocations.second.first;
        auto const & minusStrandLoc = kmerLocations.second.second;

        csvFile << kmerPos.i1 << ',' << kmerPos.i2 << ';';

        uint64_t i = 0;
        for (auto const & fastaFile : fastaFiles)
        {
            bool subsequentIterations = false;
            while (i < plusStrandLoc.size() && plusStrandLoc[i].i1 <= fastaFile.second)
            {
                if (subsequentIterations)
                    csvFile << '|'; // separator for multiple locations in one column
                csvFile << plusStrandLoc[i].i1 << ',' << plusStrandLoc[i].i2;
                subsequentIterations = true;

                ++i;
            }
        }

        if (searchParams.revCompl)
        {
            csvFile << ';';
            uint64_t i = 0;
            for (auto const & fastaFile : fastaFiles)
            {
                bool subsequentIterations = false;
                while (i < minusStrandLoc.size() && minusStrandLoc[i].i1 <= fastaFile.second)
                {
                    if (subsequentIterations)
                        csvFile << '|'; // separator for multiple locations in one column
                    csvFile << minusStrandLoc[i].i1 << ',' << (minusStrandLoc[i].i2);
                    subsequentIterations = true;

                    ++i;
                }
            }
        }
        csvFile << '\n';
    }

    csvFile.close();
}
