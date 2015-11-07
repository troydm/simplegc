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

g = {
:n=>60,
:c=>15000,
:f=>32,
:r=>10,
:s=>20,
:d=>5
}

def parse_arg(n,i)
    if ARGV[i] == n
        return ARGV[i+1].to_i
    end
end

ARGV.count.times do |i|
    g.keys.each do |s|
        if parse_arg("-"+s.to_s,i)
            g[s] = parse_arg("-"+s.to_s,i)
        end
    end
end

if (g[:s] + g[:r]) > 100
    g[:s] = 80
    g[:r] = 20
end

i=0
last=0
objs = {}
roots = []
survivors = []
obj_map = {}

def generate_random_except(from,till,except=[])
    if till <= except.size
        return nil
    end
    rnd = Random.rand(from..(till-1))
    while except.include?(rnd)
        rnd += 1
        if rnd >= till
            rnd = from
        end
    end
    return rnd
end

def percent(p,v)
    return ((p.to_f/100.0) * v).to_i
end

g[:n].times do |ni|
    puts "# iteration #{ni} (#{last} - #{last+g[:c]})"
    from = last
    g[:c].times do |ci|
        f = Random.rand(g[:f]+1)
        print "#{last}=#{f}"
        objs[last] = f
        last += 1
        i += 1
        if (i % 50) == 0
            print "\n"
        else
            print " "
        end
    end
    till = last
    # deleting previously generated roots
    del_roots = percent(g[:d],g[:c])
    puts "\n# deleting previous roots randomly #{del_roots}"
    del_roots.times do
        root = roots.sample
        if root.nil?
            next
        end
        roots.delete(root)
        print "-#{root}"
        i += 1
        if (i % 50) == 0
            print "\n"
        else
            print " "
        end
    end
    # generate roots
    gen_roots = percent(g[:r],g[:c])
    puts "\n# generating roots #{gen_roots}"
    gen_roots.times do
        root = generate_random_except(from,till,roots)
        if root.nil?
            puts "root is nill OMGZFG #{from} #{till} #{roots}"
        end
        print "+#{root}"
        i += 1
        if (i % 50) == 0
            print "\n"
        else
            print " "
        end
        roots << root
    end
    # generate survivors
    survivors.clear
    gen_survivors = percent(g[:s],g[:c])
    gen_survivors -= gen_roots
    puts "\n# generating survivors #{gen_survivors}"
    gen_survivors.times do
        survivor = generate_random_except(from,till,survivors+roots)
        field = nil
        become_root = false

        root = (roots+survivors).sample
        roots_tried = []
        while true
            if root.nil? or roots_tried.include?(root)
                root = (roots+survivors).sample
                next
            end
            if obj_map.has_key?(root)
                if objs[root] != obj_map[root].count
                    field = obj_map[root].count
                    obj_map[root] << survivor
                    break
                else
                    roots_tried << root
                    root = obj_map[root].sample
                    next
                end
            else
                if objs[root] > 0
                    obj_map[root] = [survivor]
                    field = 0
                    break
                end
            end
            roots_tried << root
            root = (roots+survivors).sample
            while roots_tried.include?(root) and roots_tried.count != roots.count
                root = (roots+survivors).sample
            end
            if (roots_tried.count+survivors.count) == roots.count
                # we don't have enough roots, we need to generate a new one
                print "+#{survivor}"
                i += 1
                if (i % 50) == 0
                    print "\n"
                else
                    print " "
                end
                become_root = true
                roots << survivor
                break
            end
        end
        if become_root
            next
        end
        print "#{root}[#{field}]=#{survivor}"
        i += 1
        if (i % 50) == 0
            print "\n"
        else
            print " "
        end
        survivors << survivor
    end
    puts "\n# running gc for iteration #{ni}"
    print "gc\n"
end
