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

#include <stdio.h>
#include <unistd.h>
#include "gc.h"


int main(int argc,char** argv){

    // initialize gc object class
    gc_object_class cls;
    cls.gc_mark_black = &gc_object_mark_black;
    cls.gc_contains = &gc_object_contains;
    cls.gc_finalize = &gc_object_finalize_debug;

    // initialize gc configuration
    gc_config config;
    gc_gen_config c[1];
    c[0].refresh_interval = 1;
    c[0].promotion_interval = 1;
    config.gens_count = 1;
    config.gens = c;
    config.pause_threshold = 100; // 100 objects
    config.max_pause = 200000000; // 200 milliseconds
    gc_init(&config);

    for(uint8_t j = 0; j < 3; ++j){
        gc_object* o;
        gc_object* o1;
        gc_object* o2;
        for(uint16_t i = 0; i < 100; ++i){
            o = gc_alloc(12);
            o->class = &cls;
            o2 = gc_alloc(8);
            o2->class = &cls;
            gc_set_ref(o,0,o2);
            gc_object* o1 = gc_alloc(12);
            o1->class = &cls;
            gc_add_root(o);
        }
        gc();
        gc_set_ref(o,0,null);
    }

    gc_destroy();

    return 0;
}
