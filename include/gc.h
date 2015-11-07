/*
The MIT License (MIT)

Copyright (c) 2015 Dmitry "troydm" Geurkov (d.geurkov@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef GC_H
#define GC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define null 0

struct gc_object_class_t;

// gc object
typedef struct gc_object_t {
    struct gc_object_t** gc_prev;
    struct gc_object_t* gc_next;
    uint32_t gc_mark;
    struct gc_object_class_t* class;    
    uint16_t refs_count;
} gc_object;

// generation config
typedef struct {
    uint64_t refresh_interval; // generation refresh interval in nanoseconds
    uint64_t promotion_interval; // generation promotion interval in nanoseconds
    // for internal use only
    uint64_t refresh_time; // last refresh time
    uint64_t promotion_time; // last promotion time
    uint32_t cycle_refreshed; // last cycle refreshed objects
    uint32_t cycle_promoted; // last cycle promoted objects
} gc_gen_config;

// gc config
typedef struct {
    uint64_t max_pause; // gc max pause in nanoseconds
    uint32_t pause_threshold; // number of objects checked after which gc pause check should occur
    uint8_t gens_count; // number of generations
    gc_gen_config* gens; // generation configs array
    // for internal use only
    // cycle related info variables
    uint64_t cycle_time; // start time of last gc collection cycle in nano seconds, used to measure passed time
    uint64_t cycle_duration; // cycle duration in nanoseconds
    uint32_t cycle_threshold; // number of objects in threshold during last cycle
    uint32_t cycle_objects; // number of objects checked during last cycle
    uint32_t cycle_collected; // number of collected objects during last cycle
    bool     cycle_full; // last cycle full
} gc_config;

// gc object class
typedef struct gc_object_class_t {
    void (*gc_mark_black)(gc_object* obj); // this marks  object from grey to black
    bool (*gc_contains)(gc_object* obj, gc_object* ref); // this checks if object contains reference object
    void (*gc_finalize)(gc_object* obj); // this is called before object is deallocated
} gc_object_class;


// initialize garbage collector
void gc_init(gc_config* config);

// deinitialize garbage collector
void gc_destroy();

// allocate gc_object
gc_object* gc_alloc(uint32_t refs_count);

// set object reference to another object
void gc_set_ref(gc_object* obj, uint16_t ref_index, gc_object* ref);

// collect garbage
uint64_t gc();

// add gc root
void gc_add_root(gc_object* obj);

// remove gc root
void gc_remove_root(gc_object* obj);

// gc object mark black
void gc_object_mark_black(gc_object* obj);

// this checks if object contains reference object
bool gc_object_contains(gc_object* obj, gc_object* ref);

// gc object finalize
void gc_object_finalize(gc_object* obj);
void gc_object_finalize_debug(gc_object* obj);

// print inner gc memory layout
void gc_print();
void gc_print_object(gc_object* obj);

// get time in nano seconds
uint64_t get_nanotime();

// for debug and internal use only
// use with caution
// get current gc configuration
gc_config* gc_get_config();

// check if object is managed by gc
bool gc_contains(gc_object* obj);

#ifdef __cplusplus
}
#endif

#endif
