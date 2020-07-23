#include "rc.h"
#include <stdio.h>
#include <string.h>

struct reference_graph {
    struct graph_entry *allocations;
    size_t n_allocations; // number of current allocations
    size_t size; // allocations len (not in bytes)
    size_t next_entry_id; // next available entry id
};

struct graph_entry {
    size_t entry_id; // unique id
    struct strong_ref *ref; // strong reference to the allocation
};


static struct reference_graph *graph = NULL;

static void *safe_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        perror("malloc failed\n");
        exit(2);
    }
    return p;
}

static void *safe_realloc(void *old, size_t size) {
    void *p = realloc(old, size);
    if (!p) {
        perror("realloc failed\n");
        exit(2);
    }
    return p;
}

/* if global graph has allocations, nothing is done */
void init_reference_graph() {
    graph = (struct reference_graph *) safe_malloc(sizeof(struct reference_graph));
    graph->allocations = (struct graph_entry *) safe_malloc(RC_INIT_SZ * sizeof(struct graph_entry));
    graph->size = RC_INIT_SZ;
    graph->n_allocations = 0;
    graph->next_entry_id = 0;
    return;
}


struct graph_entry *get_graph_entry_from_ref(struct strong_ref *ref) {
    if (!graph || !ref) {
        return NULL;
    }

    for (size_t i = 0; i < graph->n_allocations; ++i) {
        if (graph->allocations[i].ref == ref) {
            return &graph->allocations[i];
        }
    }
    return NULL;
}

struct graph_entry *get_graph_entry_by_id(size_t entry_id) {
    if (!graph) {
        return NULL;
    }

    for (size_t i = 0; i < graph->n_allocations; ++i) {
        if (graph->allocations[i].entry_id == entry_id) {
            return &graph->allocations[i];
        }
    }
    return NULL;
}

struct strong_ref *allocate(size_t ref_size, struct strong_ref* dep) {
    struct strong_ref *sr = safe_malloc(sizeof(struct strong_ref));
    sr->entry.count = 1;
    sr->ptr = safe_malloc(ref_size);

    struct graph_entry *dep_g_entry = get_graph_entry_from_ref(dep);
    if (dep && dep_g_entry) {
        sr->entry.dep_list = safe_malloc(sizeof(size_t));
        sr->entry.dep_list[0] = dep_g_entry->entry_id;
        sr->entry.n_deps = 1;
        sr->entry.count = dep->entry.count;
    } else {
        sr->entry.dep_list = NULL;
        sr->entry.n_deps = 0;

    }
    return sr;
}

struct graph_entry *new_graph_entry(size_t n, struct strong_ref* dep) {
    if (!graph) {
        init_reference_graph();
    }

    // realloc
    if (graph->size == graph->n_allocations) {
        graph->allocations = (struct graph_entry *) safe_realloc(graph->allocations, sizeof(struct graph_entry) * graph->size * RC_GROWTH_RT);
        graph->size *= RC_GROWTH_RT;
    }

    // return next available entry, incrament n allocations
    struct graph_entry *new_entry =  &graph->allocations[graph->n_allocations++];
    new_entry->entry_id = graph->next_entry_id++;
    new_entry->ref = allocate(n, dep);

    return new_entry;
}



void delete_entry(size_t index) {
    free(graph->allocations[index].ref->entry.dep_list);
    free(graph->allocations[index].ref->ptr);
    free(graph->allocations[index].ref);
    memmove(graph->allocations + index, graph->allocations + index + 1, graph->n_allocations - index - 1);

    graph->n_allocations--;
    return;
}


void decrament_count(struct graph_entry *g_entry) {
    if (!g_entry || !g_entry->ref) {
        return;
    }

    // decrament any dependencies
    for (size_t i = 0; i < graph->n_allocations; ++i) {
        for (size_t j = 0; j < graph->allocations[i].ref->entry.n_deps; ++j) {
            if (graph->allocations[i].ref->entry.dep_list[j] == g_entry->entry_id) {
                decrament_count(&graph->allocations[i]);
            }
        }
    }
    g_entry->ref->entry.count--;
    if (g_entry->ref->entry.count == 0) {
        delete_entry(g_entry->entry_id);
    }
    return;
}




// strong reference to existing allocation
struct strong_reference *strong_reference_to_allocation(void *ptr, struct strong_ref *dep) {
    return NULL;
}


struct strong_ref *find_reference(void *ptr) {
    // iterate over graph
    for (size_t i = 0; i < graph->n_allocations; ++i) {
        if (graph->allocations[i].ref->ptr == ptr) {
            return graph->allocations[i].ref;
        }
    }

    return NULL;
}



/**
 * Returns an allocation of n bytes and creates an internal rc entry.
 *
 * If the ptr argument is NULL and deps is NULL, it will return a new
 * allocation
 *
 * If the ptr argument is NULL and deps is not NULL, it will return
 * a new allocation but the count will correlate to the dependency
 * if the dependency is deallocated the reference count on the object will
 * decrement
 *
 * If the ptr argument is not NULL and an entry exists, it will increment
 *  the reference count of the allocation and return a strong_ref pointer
 *
 * If the ptr argument is not NULL and an entry exists and dep is not
 * NULL, it will increment the count of the strong reference but the count
 * will be related to the dependency, if the dependency is deallocated the
 * reference count on the object will decrement
 */
struct strong_ref* rc_alloc(void* ptr, size_t n, struct strong_ref* dep) {
    if (!ptr) {
        return new_graph_entry(n, dep)->ref; // deps arnt accounted for
    } else {
        struct strong_ref *sr = find_reference(ptr);
        if (sr) {
            sr->entry.count++;
            return sr;
        }
        // undefined so far
    }
    return NULL;
}

/**
 * Downgrades a strong reference to a weak reference, this will decrement the
 * reference count by 1
 * If ref is NULL, the function will return an invalid weak ref object
 * If ref is a value that does not exist in the reference graph, it will return
 * an weak_ref object that is invalid
 *
 * If ref is a value that does exist in the reference graph, it will return
 *    a valid weak_ref object
 *
 * An invalid weak_ref object is where its entry_id field is set to
 *   0xFFFFFFFFFFFFFFFF
 *
 * @param strong_ref* ref (reference to allocation)
 * @return weak_ref (reference with an entry id)
 */
struct weak_ref rc_downgrade(struct strong_ref* ref) {
    struct weak_ref r = { 0xFFFFFFFFFFFFFFFF };
    if (!graph || !ref) {
        return r;
    }

    struct graph_entry *g_entry = get_graph_entry_from_ref(ref);
    if (!g_entry) {
        return r;
    }

    decrament_count(g_entry);

    if (graph->n_allocations < (graph->size / RC_GROWTH_RT)) {
        graph->allocations = (struct graph_entry *) safe_realloc(graph->allocations, sizeof(struct graph_entry) * (graph->size / RC_GROWTH_RT));
        graph->size /= RC_GROWTH_RT;
    }

    // check if still valid entry, after de
    if ((g_entry = get_graph_entry_from_ref(ref))) {
        r.entry_id = g_entry->entry_id;
    }

    return r;
}


/**
 * Upgrdes a weak reference to a strong reference.
 * The weak reference should check that the entry id is valid (bounds check)
 * If a strong reference no longer exists or has been deallocated, the return
 *   result should be null.
 */
struct strong_ref* rc_upgrade(struct weak_ref ref) {
    if (!graph || ref.entry_id < 0 || ref.entry_id >= graph->next_entry_id) {
        return NULL;
    }

    struct graph_entry *g_entry = get_graph_entry_by_id(ref.entry_id);
    if (!g_entry) {
        return NULL;
    }

    // exists
    g_entry->ref->entry.count++;
    return g_entry->ref;
}


/**
 * Cleans up the reference counting graph.
 */
void rc_cleanup() {
    if (!graph) {
        return;
    }


    for (size_t i = 0; i < graph->n_allocations; ++i) {
        free(graph->allocations[i].ref->entry.dep_list);
        free(graph->allocations[i].ref->ptr);
        free(graph->allocations[i].ref);
    }

    free(graph->allocations);
    free(graph);
    graph = NULL;
    return;
}
