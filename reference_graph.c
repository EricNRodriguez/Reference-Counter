////
//// Created by eric on 11/4/20.
////
//
//#include "reference_graph.h"
//#include "rc.h"
//
//
//struct reference_graph {
//    struct entry *allocations;
//    size_t n_allocations; // number of current allocations
//    size_t size; // allocations len (not in bytes)
//    size_t next_entry_id; // next available entry id
//};
//
//struct entry {
//    size_t entry_id; // unique id
//    struct strong_ref *ref; // strong reference to the allocation
//};
//
//
