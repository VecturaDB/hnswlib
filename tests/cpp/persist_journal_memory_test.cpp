// The journal of a save built in memory (takePersistJournal) leaves no memory in the arenas of glibc, whatever the mmap
// threshold of glibc is when the save runs.
#include "../../hnswlib/hnswlib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#if defined(__linux__) && defined(__GLIBC__)
#include <malloc.h>
#include <unistd.h>

int main()
{
    // Once glibc has freed a block mapped on its own, it serves the blocks below that size from its arenas, which keep
    // them after the free. Any library of the process can free such a block: here a block of 30 MB.
    char *large = (char *)malloc(30 << 20);
    memset(large, 1, 30 << 20);
    free(large);

    char location[] = "/tmp/persist_journal_memory_XXXXXX";
    if (mkdtemp(location) == nullptr)
    {
        perror("mkdtemp");
        return 1;
    }
    const int d = 64;
    const size_t n = 40000;
    hnswlib::L2Space space(d);
    hnswlib::HierarchicalNSW<float> index(&space, n, 16, 100, 0, false, false, true, location);
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> distrib(0, 1);
    std::vector<float> point(d);
    for (size_t i = 0; i < n; i++)
    {
        for (int j = 0; j < d; j++)
            point[j] = distrib(rng);
        index.addPoint(point.data(), i);
    }
    const std::vector<std::pair<std::string, std::string>> whole_files = {{"whole", std::string(4 << 20, 'w')}};

    size_t before = mallinfo2().arena;
    size_t journal_size = 0;
    for (int i = 0; i < 5; i++)
    {
        // The whole index is dirty: the journal holds it all, about 20 MB.
        auto journal = index.takePersistJournal(whole_files);
        journal_size = journal.size();
        index.restoreTakenElements();
    }
    size_t after = mallinfo2().arena;

    std::string command = std::string("rm -rf ") + location;
    if (system(command.c_str()) != 0)
        fprintf(stderr, "Cannot remove %s\n", location);
    if (journal_size < (16 << 20))
    {
        fprintf(stderr, "The journal holds %zu bytes, less than the index\n", journal_size);
        return 1;
    }
    if (after >= before + (8 << 20))
    {
        fprintf(stderr, "The arenas of glibc grew from %zu to %zu bytes\n", before, after);
        return 1;
    }
    printf("The arenas of glibc went from %zu to %zu bytes, with journals of %zu bytes\n", before, after, journal_size);
    return 0;
}
#else
int main()
{
    printf("Not glibc on Linux: nothing to check\n");
    return 0;
}
#endif
