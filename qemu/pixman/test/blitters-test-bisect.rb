#!/usr/bin/env ruby

if not ARGV[0] or not ARGV[1] then
    printf("Please provide two 'blitters-test' static binaries in the command line.\n\n")
    printf("The first should be linked with a correct reference pixman library.\n")
    printf("The second binary should be linked with a pixman library which needs to be tested.\n")
    exit(0)
end

def test_range(min, max)
    if `#{ARGV[0]} #{min} #{max} 2>/dev/null` == `#{ARGV[1]} #{min} #{max} 2>/dev/null` then
        return
    end
    while max != min + 1 do
        avg = ((min + max) / 2).to_i
        res1 = `#{ARGV[0]} #{min} #{avg} 2>/dev/null`
        res2 = `#{ARGV[1]} #{min} #{avg} 2>/dev/null`
        if res1 != res2 then
            max = avg
        else
            min = avg
        end
    end
    return max
end

base = 1
while true do
    # run infinitely, processing 100000 test cases per iteration
    printf("running tests %d-%d\n", base, base + 100000 - 1);
    res = test_range(base, base + 100000 - 1)
    if res then
        printf("-- ref --\n")
        printf("%s\n", `#{ARGV[0]} -#{res}`)
        printf("-- new --\n")
        printf("%s\n", `#{ARGV[1]} -#{res}`)

        printf("\nFailed test %d, you can reproduce the problematic conditions by running\n", res)
        printf("#{ARGV[1]} -%d\n", res)
        exit(1)
    end
    base += 100000
end
