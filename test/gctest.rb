#!/usr/bin/env ruby
# The MIT License (MIT)
#
# Copyright (c) 2015 Dmitry "troydm" Geurkov (d.geurkov@gmail.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


class GCObject < Hash
    @@last_id = 0
    
    attr_accessor :id
    attr_accessor :array_id

    def initialize(refs)
        super
        @id = @@last_id
        @@last_id += 1
        @refs = refs
        @root_ref_count = 0
        @array_id = 0
    end

    def set_ref(ref_ind,ref,linei)
        if ref_ind >= @refs
            puts "invalid reference index #{ref_ind} on #{@array_id}[#{ref_ind}]=#{ref.array_id} on line: #{linei}"
            exit
        end
        if ref.nil?
            delete(ref_ind)
        else
            self[ref_ind] = ref
        end
    end

    def is_root
        return @root_ref_count > 0
    end

    def inc_root_ref_count
        @root_ref_count += 1
    end

    def dec_root_ref_count(linei)
        if @root_ref_count == 0
            puts "trying to decrease zero root reference count object on -#{@array_id} line: #{linei}"
            exit
        end
        @root_ref_count -= 1
    end
end

class GCTest

    def initialize(testfile)
        @filename = testfile
        @testfuns = ""
        @testcode = ""
        all_objs = {}
        objs = {}
        @linei=1
        File.open(@filename) do |f|
            f.each_line do |l|
                if l[0] == "#" or l.strip.size == 0
                    @linei += 1
                    next
                end
                l.split(' ').each do |i|
                    case i 
                        when /w\((\d+)\)/
                            @testcode << "millisleep(#{$1});\n"
                        when /gc/
                            @testcode << "run_gc();\n"
                            gc(all_objs,objs)
                        when /s/
                            @testcode << "gc_print();\n"
                        when /e/
                            @testcode << "end(); return 0;\n"
                            return
                        when /(\d+)\[(\d+)\]=(\d+)/
                            if !objs.has_key?($1.to_i)
                                puts "invalid object index: #{$1} on line: #{@linei}"
                                exit
                            end
                            if !objs.has_key?($3.to_i)
                                puts "invalid object index: #{$3} on line: #{@linei}"
                                exit
                            end
                            @testcode << "gc_set_ref(array_get(objects,#{$1}),#{$2},array_get(objects,#{$3}));\n"
                            objs[$1.to_i].set_ref($2.to_i,objs[$3.to_i],@linei)
                        when /(\d+)\[(\d+)\]/
                            if !objs.has_key?($1.to_i)
                                puts "invalid object index: #{$1} on line: #{@linei}"
                                exit
                            end
                            @testcode << "gc_set_ref(array_get(objects,#{$1}),#{$2},null);\n"
                            objs[$1.to_i].set_ref($2.to_i,nil,@linei)
                        when /(\d+)=(\d+)/
                            if objs.has_key?($1.to_i)
                                puts "invalid object definition for #{$1}=#{$2} on line: #{@linei}"
                                puts "object already defined"
                                exit
                            end
                            objs[$1.to_i] = GCObject.new($2.to_i)
                            objs[$1.to_i].array_id = $1.to_i
                            all_objs[objs[$1.to_i].id] = objs[$1.to_i]
                            @testcode << "object_create(#{objs[$1.to_i].id},#{$1},#{$2});\n"
                        when /([-\+])(\d+)/
                            if $1 == '+'
                                if !objs.has_key?($2.to_i)
                                    puts "invalid object index: #{$2} on line: #{@linei}"
                                    exit
                                end
                                @testcode << "gc_add_root(array_get(objects,#{$2}));\n"
                                objs[$2.to_i].inc_root_ref_count
                            else
                                if !objs.has_key?($2.to_i)
                                    puts "invalid object index: #{$2}"
                                    exit
                                end
                                @testcode << "gc_remove_root(array_get(objects,#{$2}));\n"
                                objs[$2.to_i].dec_root_ref_count(@linei)
                            end
                        when /(\d+)/
                            if !objs.has_key?($1.to_i)
                                puts "invalid object definition removal for #{$1} on line: #{@linei}"
                                puts "object is not defined"
                                exit
                            end
                            @testcode << "array_set(objects,#{$1},null);\n"
                            objs.delete($1.to_i)
                    end
                end
                @linei += 1
            end
        end
        @testfuns << "void check_objs(){\n  gc_object* obj; uint32_t s = 0;"
        black = gc(all_objs,objs)
        all_objs.values.each do |o|
            if !black.include?(o.id)
                @testcode << "array_set(all_objects,#{o.id},null);\n"
            end
        end
        @testfuns << """for(uint32_t i = 0; i < all_objects->size; ++i){
                        obj = (gc_object*)array_get(all_objects,i);
                        if(obj != null){
                            if(!gc_contains(obj)){
                                printf(\"object %d %p not found, incorrect gc behaviour, test failed\\n\",i,obj);
                            }else{
                                ++s;
                            }
                        }
        }

        """
        @testfuns << """ printf(\"checked objects that survived: %d\\n\",s);
                      }

        """
        rc = black.size
        puts "total object allocations: #{all_objs.size.to_s}"
        puts "expected number of objects to survive gc: #{rc.to_s}"
        @testcode << "total_objects_allocated = #{all_objs.size.to_s};"
        @testcode << "expected_survivors_count = #{rc.to_s};"
    end

    def gc(objs,objs_to_remove)
        black = []
        objs.values.each do |r|
            if r.is_root
                black << r.id
            end
        end
        grey = Array.new(black)
        rc = grey.size
        while grey.size > 0
            r = grey.pop
            objs[r].values.each do |i|
                if !black.include?(i.id)
                    grey << i.id
                    black << i.id
                    rc += 1
                end
            end
        end
        objs_to_remove.keep_if {|k,v| black.include?(v.id)}
        return black
    end
        

    def rungctest()
        include="""
            #include <stdio.h>
            #include \"gctest.h\"
            #include \"gc.h\"

            extern gc_object_class cls;
            extern uint32_t expected_survivors_count;
            extern uint32_t total_objects_allocated;
            extern uint32_t garbage_collected_count;
            extern uint32_t survivors_count;

            extern uint64_t total_gc_time;
            extern array* objects;
            extern array* all_objects;
            extern uint64_t t;
        """

        t = 1
        tests = ["gctest1"]
        f = File.open(tests[0]+".c",'w')
        f.puts "#{include}\n"
        f.puts "void gctest1(){"
        i = 1
        @testcode.split("\n").each do |l|
            if i % 60000 == 0
                f.puts "}"
                f.close
                system("gcc -std=gnu99 -O2 -I../include -Wall -g -c ./"+tests[tests.size-1]+".c")
                t += 1
                tests << "gctest#{t.to_s}"
                f = File.open(tests[tests.size-1]+".c",'w')
                f.puts "#{include}\n"
                f.puts "void gctest#{t}(){"
            end
            f.puts(l)
            i += 1
        end
        f.puts "}"
        f.close
        system("gcc -std=gnu99 -O2 -I../include -Wall -g -c ./"+tests[tests.size-1]+".c")
        @testcode = ""
        tests.each do |t|
            @testcode << "#{t}();\n"
            @testfuns << "void #{t}();\n"
        end
        gcobj = "../build/src/CMakeFiles/simplegc.dir/gc.c.o"
        s = ""
        s << "
            #{include}

            #{@testfuns}
            
            int main(int argc, char** argv){
                // initialize gc configuration
                initialize_gc();
                // initialize some objects
                objects = array_create(10000);
                all_objects = array_create(10000);

                printf(\"running gc test: "+@filename+"\\n\");
                t = get_nanotime();

                #{@testcode}

                end();
                return 0;
            }"

        File.open("gctestmain.c",'w') do |f|
            f.puts s
        end

        lds = tests.map {|t| t+".o "}.join
        system("gcc -std=gnu99 -O2 -I../include -Wall -g -c ./gctest.c")
        system("gcc -std=gnu99 -O2 -I../include -Wall -g -c ./gctestmain.c")
        system("gcc -lrt -o gctest #{gcobj} ./gctest.o ./gctestmain.o #{lds}")
        system("./gctest")

        File.delete("gctest.o")
        File.delete("gctestmain.c")
        File.delete("gctestmain.o")
        tests.each do |t|
            File.delete(t+".c")
            File.delete(t+".o")
        end
    end

end

if !ARGV[0].nil? and  File.file?(ARGV[0])
    GCTest.new(ARGV[0]).rungctest
else
    puts "please specify a test file as first argument"
end
