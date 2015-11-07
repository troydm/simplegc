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

#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include "gctest.h"
#include "gc.h"


void millisleep(uint32_t millis){
    struct timespec t;
    t.tv_sec = millis/1000;
    t.tv_nsec = (millis%1000)*1000000;
    nanosleep(&t,null);
}

array* array_create(uint32_t initial_size){
    array* a = (array*)malloc(sizeof(array));
    a->size = initial_size;
    a->data = (void**)malloc(sizeof(void*)*initial_size);
    memset(a->data,0,a->size*sizeof(void*));
    return a;
}

void array_free(array* a){
    free(a->data);
    free(a);
}

void array_set(array* a, uint32_t i, void* v){
    if(i >= a->size){
        uint32_t ps = a->size;
        a->size = i+1000;
        a->data = (void**)realloc(a->data,sizeof(void*)*a->size);
        memset(a->data+ps,0,(a->size - ps)*sizeof(void*));
    }
    a->data[i] = v;
}

void* array_get(array* a, uint32_t i){
    if(i >= a->size)
        return null;
    return a->data[i];
}

gc_object_class cls;
uint32_t expected_survivors_count = 0;
uint32_t total_objects_allocated = 0;
uint32_t garbage_collected_count = 0;
uint32_t survivors_count = 0;
uint32_t total_gc_calls = 0;

void garbage_collected_finalize(gc_object* obj){
    garbage_collected_count += 1;
}

void initialize_gc(){
    // initialize gc object class
    cls.gc_mark_black = &gc_object_mark_black;
    cls.gc_contains = &gc_object_contains;
    cls.gc_finalize = &garbage_collected_finalize;

    // initialize gc configuration
    gc_config config;
    gc_gen_config c[3];
    c[0].refresh_interval = 500000ull; // 0.5 millis
    c[0].promotion_interval = 1000000ull; // 1 millis
    c[1].refresh_interval = 2000000ull; // 2 millis
    c[1].promotion_interval = 6000000ull; // 6 millis
    c[2].refresh_interval = 250000000ull; // 15 millis
    c[2].promotion_interval = 0; // not needed for last generation
    config.gens_count = 3;
    config.gens = c;
    config.pause_threshold = 100; // 100 objects
    config.max_pause = 200000000; // 200 milliseconds
    gc_init(&config);
}

void survivors_finalize(gc_object* obj){
    survivors_count += 1;
}

uint64_t total_gc_time = 0;
array* objects;
array* all_objects;
uint64_t t;

void check_objs();

void run_gc_no_total(){
    gc();
    gc_config* c = gc_get_config();
    printf("gc collected %d objects took %.2f millis [",c->cycle_collected,((double)c->cycle_duration)/1000000);
    for(uint8_t i = 0; i < c->gens_count; ++i){
        printf(" %d/%d",c->gens[i].cycle_promoted, c->gens[i].cycle_refreshed);
    }
    printf(" ]\n");
}

void end(){
    printf("test ended, took %.2f millis\n",(((double)(get_nanotime()-t))/1000000));
    // force promotion of objects to last generation
    gc_config* config = gc_get_config();
    for(uint8_t i = 0; i < config->gens_count; ++i){
        config->gens[i].promotion_interval = 1;
    }
    config->gens[config->gens_count-1].refresh_interval = 1;

    run_gc_no_total();
    config->gens[config->gens_count-1].refresh_interval = 1000000000ull;

    while(!config->cycle_full){
        run_gc_no_total();
    }

    check_objs();

    array_free(all_objects);
    array_free(objects);
    // register new finalizer for survivors counting
    cls.gc_finalize = &survivors_finalize;
    gc_destroy();
    printf("garbage collected %d\n",garbage_collected_count);
    printf("actual survivors %d\n",survivors_count);
    if(survivors_count+garbage_collected_count != total_objects_allocated)
        printf("total objects don't much with actual survivors + garbage collected objects\n");
    if(survivors_count != expected_survivors_count)
        printf("expected survivors don't much with actual survivors %d != %d\n",expected_survivors_count,survivors_count);
    else
        printf("expected survivors match with actual survivors\n");
    printf("total gc calls: %d\n",total_gc_calls);
    printf("total time spent in gc: %.2f millis\n",((double)total_gc_time)/1000000);
}

void object_create(uint32_t gid, uint32_t id, uint32_t f){
    gc_object* obj = gc_alloc(f);
    array_set(objects,id,obj);
    obj->class = &cls;
    array_set(all_objects,gid,obj);
}

void run_gc(){
    total_gc_calls += 1;
    total_gc_time += gc();
    gc_config* c = gc_get_config();
    printf("gc collected %d objects took %.2f millis [",c->cycle_collected,((double)c->cycle_duration)/1000000);
    for(uint8_t i = 0; i < c->gens_count; ++i){
        printf(" %d/%d",c->gens[i].cycle_promoted, c->gens[i].cycle_refreshed);
    }
    printf(" ]\n");
}
