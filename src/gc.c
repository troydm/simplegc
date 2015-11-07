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

#ifdef __cplusplus
extern "C" {
#endif

#include "gc.h"
#include <time.h>
#include <malloc.h>
#include <errno.h>

#define WHITE 0
#define GREY  1
#define BLACK 2
#define SILVER 3

// gc configuration
static gc_config conf;
// gc list variables
static gc_object* transparent = null;
static gc_object* white = null;
static gc_object* silver = null;
static gc_object* grey = null;
static gc_object** black;

// initialize garbage collector
void gc_init(gc_config* config){
    // number of generations can't be more than 64 or equal to 0
    if(config->gens_count == 0 || config->gens_count > 64){
        errno = EINVAL;
        return;
    }
    // copy config
    conf = *config;
    conf.gens = (gc_gen_config*)malloc(sizeof(gc_gen_config) * conf.gens_count);    
    black = (gc_object**)malloc(sizeof(gc_object*) * conf.gens_count);
    // foreach generation config
    for(uint8_t i = 0; i < conf.gens_count; ++i){
        // copy config and initialize generation
        conf.gens[i] = config->gens[i];
        conf.gens[i].refresh_time = get_nanotime();
        conf.gens[i].promotion_time = conf.gens[i].refresh_time;
        black[i] = null;
    }
}

// completely free entire list finalizing all objects inside
inline void gc_free_list(gc_object* list){
    while(list != null){
        gc_object* obj = list;
        list = list->gc_next; 
        (obj->class->gc_finalize)(obj);
        free(obj);
    }
}

// deinitialize garbage collector
void gc_destroy(){
    // free all objects from all lists
    gc_free_list(transparent);
    gc_free_list(white);
    gc_free_list(silver);
    gc_free_list(grey);
    for(uint8_t i = 0; i < conf.gens_count; ++i)
        gc_free_list(black[i]);
    // remove black list and generation configs
    free(black);
    free(conf.gens);
}

// gc mark
#define gc_set_mark(o,r,gi) o->gc_mark = (r << 8) | gi
// generation part
#define gc_gen_part(o) (o->gc_mark & 0xFF)
#define gc_gen_num(o) (o->gc_mark & 0x3F)
#define gc_gen_set(o,g) gc_set_mark(o,gc_root_ref_count(o), gc_color_bit(o) | (g & 0x3F))
// color bits
#define gc_color_bit(o) (o->gc_mark & 0xC0)
#define gc_color(o) (gc_gen_part(o) >> 6)
#define gc_color_is_white(o) (gc_color_bit(o) == 0x00)
#define gc_color_is_grey(o) (gc_color_bit(o) == 0x40)
#define gc_color_is_black(o) (gc_color_bit(o) == 0x80)
#define gc_color_is_silver(o) (gc_color_bit(o) == 0xC0)
#define gc_color_is_silver_or_white(o) (gc_color_is_silver(o) || gc_color_is_white(o))
#define gc_mark_white(o) o->gc_mark &= 0xFFFFFF3F
#define gc_mark_grey(o) gc_mark_white(o); o->gc_mark |= 0x40
#define gc_mark_black(o) gc_mark_white(o); o->gc_mark |= 0x80
#define gc_mark_silver(o) o->gc_mark |= 0xC0
// root reference count
#define gc_root_ref_count(o) (o->gc_mark >> 8)
#define gc_inc_root_ref_count(o) gc_set_mark(o,(gc_root_ref_count(o)+1),gc_gen_part(o))
#define gc_dec_root_ref_count(o) gc_set_mark(o,(gc_root_ref_count(o)-1),gc_gen_part(o))

// add object to list
inline void gc_list_add(gc_object** list, gc_object* obj){
    obj->gc_next = *list;
    if(obj->gc_next != null)
        obj->gc_next->gc_prev = &(obj->gc_next);
    obj->gc_prev = list;
    *list = obj;
}

// remove object from current list
inline void gc_list_remove(gc_object* obj){
    *(obj->gc_prev) = obj->gc_next;
    if(obj->gc_next != null)
        obj->gc_next->gc_prev = obj->gc_prev;
}

// move object to list
inline void gc_list_move(gc_object* obj, gc_object** list){
    gc_list_remove(obj);
    gc_list_add(list,obj);
}

// move all objects from list to list
inline void gc_list_move_all(gc_object** from, gc_object** to){
    if(*to != null){
        gc_object* obj = *from;
        while(obj->gc_next != null)
            obj = obj->gc_next;
        obj->gc_next = *to;
        (*to)->gc_prev = &(obj->gc_next);
    }
    *to = *from;
    (*to)->gc_prev = to;
    *from = null;
}

// add gc root
void gc_add_root(gc_object* obj){
    // increase root ref count
    gc_inc_root_ref_count(obj);
    // mark object as grey if it's white or silver
    if(gc_color_is_silver_or_white(obj)){
        gc_list_move(obj,&grey);
        gc_mark_grey(obj);
    }
}

// remove gc root
void gc_remove_root(gc_object* obj){
    // decrease root ref count
    if(gc_root_ref_count(obj) != 0)
        gc_dec_root_ref_count(obj);
}

// allocate gc_object
gc_object* gc_alloc(uint32_t refs_count){
    // allocate new white object in generation 0 with 0 root ref count
    gc_object* obj = (gc_object*)malloc(sizeof(gc_object) + refs_count*sizeof(gc_object*));

    // initialize references
    obj->refs_count = refs_count; // number of references this object might contain
    gc_object** refs = (gc_object**)(obj+1); // start of refs array
    for(uint16_t i = 0; i < refs_count; ++i)
        refs[i] = null;

    // set gc mark
    obj->gc_mark = 0; // initial gc_mark value (0 generation white color)
    // add object to white list
    gc_list_add(&white,obj);
    return obj;
}

// set object reference to another object
void gc_set_ref(gc_object* obj, uint16_t ref_index, gc_object* ref){
    gc_object** refs = (gc_object**)(obj+1); // start of refs array
    refs[ref_index] = ref;
    // if object is black because it mutated we need to mark it grey again
    if(gc_color_is_black(obj)){
        gc_list_move(obj,&grey);
        gc_mark_grey(obj);
    }
}

#define gc_cycle_check \
    if(conf.cycle_threshold >= conf.pause_threshold){ \
        if((get_nanotime() - conf.cycle_time) >= conf.max_pause){ \
            return gc_cycle_end(); \
        } \
        conf.cycle_objects += conf.cycle_threshold; \
        conf.cycle_threshold = 0; \
    }

#define gc_cycle_check_no_return \
    if(conf.cycle_threshold >= conf.pause_threshold){ \
        if((get_nanotime() - conf.cycle_time) >= conf.max_pause){ \
            return; \
        } \
        conf.cycle_objects += conf.cycle_threshold; \
        conf.cycle_threshold = 0; \
    }

// end gc cycle
static inline uint64_t gc_cycle_end(){
    conf.cycle_objects += conf.cycle_threshold;
    conf.cycle_duration = get_nanotime() - conf.cycle_time;
    return conf.cycle_duration;
}

// collect garbage
uint64_t gc(){
    // start gc cycle
    conf.cycle_time = get_nanotime();
    conf.cycle_threshold = 0;
    conf.cycle_objects = 0;
    conf.cycle_collected = 0;
    conf.cycle_full = false;

    // transparent cleanup phase before cycle
    while(transparent != null){
        (transparent->class->gc_finalize)(transparent);
        gc_object* obj = transparent;
        transparent = obj->gc_next;
        free(obj);
        conf.cycle_threshold += 11;
        conf.cycle_collected += 1;
        // check pause threshold
        gc_cycle_check
    }

    // promotion phase
    uint64_t time_now = get_nanotime();
    for(uint8_t i = 0; i < conf.gens_count; ++i){
        conf.gens[i].cycle_refreshed = 0;
        conf.gens[i].cycle_promoted = 0;
        gc_object* obj = black[i];
        if(obj != null){
            // promote generation
            if(i != (conf.gens_count-1) && time_now - conf.gens[i].promotion_time > conf.gens[i].promotion_interval){
                do{
                    gc_gen_set(obj,(i+1));
                    // move to next generation
                    gc_list_move(obj,&(black[i+1]));

                    conf.cycle_threshold += 1;
                    conf.gens[i].cycle_promoted += 1;
                    // check pause threshold
                    gc_cycle_check
                    obj = black[i];
                }while(obj != null);
                conf.gens[i].promotion_time = get_nanotime();
            }

            // refresh generation
            if(time_now - conf.gens[i].refresh_time > conf.gens[i].refresh_interval){
                while(obj != null){
                    // if has root references
                    if(gc_root_ref_count(obj) > 0){
                        // mark as grey
                        gc_list_move(obj,&grey);
                        gc_mark_grey(obj);
                    }else{
                        // mark as silver
                        gc_list_move(obj,&silver);
                        gc_mark_silver(obj);
                    }
                    conf.cycle_threshold += 1;
                    conf.gens[i].cycle_refreshed += 1;
                    // check pause threshold
                    gc_cycle_check
                    obj = black[i];
                }
                conf.gens[i].refresh_time = get_nanotime();
            }
        }
    }

    // mark phase
    while(grey != null){
        (grey->class->gc_mark_black)(grey);
        conf.cycle_threshold += 1;
        // check pause threshold
        gc_cycle_check
    }

    // mark silver phase
    while(silver != null){
        bool found = false;
        uint8_t to_gen;
        uint8_t i = conf.gens_count - 1;
        while(true){
            gc_object* obj = black[i];
            while(obj != null){
                if((obj->class->gc_contains)(obj,silver)){
                    found = true;
                    to_gen = i;
                    break;
                }
                gc_cycle_check
                obj = obj->gc_next;
            }
            if(found || i == 0)
                break;
            --i;
        }
        if(found){
            // correct generation and mark as grey
            gc_gen_set(silver,to_gen);
            gc_mark_grey(silver);
            gc_list_move(silver,&grey);
            conf.cycle_threshold += 1;
            // run mark grey phase
            while(grey != null){
                (grey->class->gc_mark_black)(grey);
                conf.cycle_threshold += 1;
                // check pause threshold
                gc_cycle_check
            }
        }else{
            // mark as white
            gc_mark_white(silver);
            gc_list_move(silver,&white);
            conf.cycle_threshold += 1;
        }
        // check pause threshold
        gc_cycle_check
    }


    // sweep phase
    if(white != null){
        // make object transparent
        gc_list_move_all(&white,&transparent);
        conf.cycle_threshold += 50;
        // check pause threshold
        gc_cycle_check
    }

    // transparent cleanup phase after cycle
    while(transparent != null){
        (transparent->class->gc_finalize)(transparent);
        gc_object* obj = transparent;
        transparent = obj->gc_next;
        free(obj);
        conf.cycle_threshold += 11;
        conf.cycle_collected += 1;
        // check pause threshold
        gc_cycle_check
    }

    // set cycle full
    conf.cycle_full = true;

    return gc_cycle_end();
}

// gc object mark black
void gc_object_mark_black(gc_object* obj){

    gc_object** refs = (gc_object**)(obj+1); // start of refs array

    // for each ref
    for(uint16_t i = 0; i < obj->refs_count; ++i){
        if(refs[i] != null && gc_color_is_silver_or_white(refs[i])){
            // mark object as grey
            gc_list_move(refs[i],&grey);
            gc_mark_grey(refs[i]);                        
            conf.cycle_threshold += 1;
            // check pause threshold
            gc_cycle_check_no_return
        }
    }

    // mark object as black
    gc_list_move(obj,&(black[gc_gen_num(obj)]));
    gc_mark_black(obj);                        
}

// this checks if object contains reference object
bool gc_object_contains(gc_object* obj, gc_object* ref){
    conf.cycle_threshold += 1;
    gc_object** refs = (gc_object**)(obj+1); // start of refs array
    for(uint16_t i = 0; i < obj->refs_count; ++i){
        if(refs[i] == ref)
            return true;
    }
    return false;
}

// this method is called on object before it's freed
void gc_object_finalize(gc_object* obj){
    // do nothing for finalize
}

// for debug only
void gc_object_finalize_debug(gc_object* obj){
    printf("finalize called on %p\n",obj);
}


inline void gc_print_object_list(gc_object** list){
    printf("[%p (%p)]: ",list,*list);
    gc_object* obj = *list;
    uint32_t i = 0;
    while(obj != null){
        ++i;
        printf("[%p (%p %p)] ",obj,obj->gc_prev,obj->gc_next);
        obj = obj->gc_next;
    }
    printf("- %d objects\n",i);
}

// print inner gc memory layout
void gc_print(){
    printf("WHITE\n");
    gc_print_object_list(&white);

    printf("SILVER\n");
    gc_print_object_list(&silver);

    printf("GREY\n");
    gc_print_object_list(&grey);

    printf("BLACK\n");
    for(uint8_t i = 0; i < conf.gens_count; ++i){
        printf("G%d",i);
        gc_print_object_list(&black[i]);
    }
}

void gc_print_object(gc_object* obj){
    char* color;
    switch(gc_color(obj)){
        case WHITE:
            color = "WHITE";
            break;
        case SILVER:
            color = "SILVER";
            break;
        case GREY:
            color = "GREY";
            break;
        case BLACK:
            color = "BLACK";
            break;
    }
    printf("%p[roots: %d color: %s gen: %d]\n",obj,gc_root_ref_count(obj),color,gc_gen_num(obj));
}

// get current gc configuration
gc_config* gc_get_config(){
    return &conf;
}

// check if object is managed by gc
bool gc_contains(gc_object* obj){
    // check white
    gc_object* o = white;
    while(o != null){
        if(o == obj)
            return true;
        o = o->gc_next;
    }

    // check silver
    o = silver;
    while(o != null){
        if(o == obj)
            return true;
        o = o->gc_next;
    }

    // check grey
    o = grey;
    while(o != null){
        if(o == obj)
            return true;
        o = o->gc_next;
    }
    
    // check black
    for(uint8_t i = 0; i < conf.gens_count; ++i){
        o = black[i];
        while(o != null){
            if(o == obj)
                return true;
            o = o->gc_next;
        }
    }

    return false;
}

// get current time in nano seconds
uint64_t get_nanotime(){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (((uint64_t)t.tv_sec)*1000000000) + ((uint64_t)t.tv_nsec);
}

#ifdef __cplusplus
}
#endif
