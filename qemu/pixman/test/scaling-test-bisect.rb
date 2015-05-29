#!/usr/bin/env ruby

if not ARGV[0] or not ARGV[1] then
    printf("Please provide two 'scaling-test' static binaries in the command line.\n\n")
    printf("The first should be linked with the correct reference pixman library.\n")
    printf("The second binrary should be linked with the pixman library to be tested.\n")
    exit(0)
end

$MAX = 3000000
$MIN = 1
$AVG = 0

if `#{ARGV[0]} #{$MAX} 2>/dev/null` == `#{ARGV[1]} #{$MAX} 2>/dev/null` then
    printf("test ok\n")
    exit(0)
end

printf("test failed, bisecting...\n")

while $MAX != $MIN + 1 do
    $AVG = (($MIN + $MAX) / 2).to_i
    res1 = `#{ARGV[0]} #{$AVG} 2>/dev/null`
    res2 = `#{ARGV[1]} #{$AVG} 2>/dev/null`
    if res1 != res2 then
        $MAX = $AVG
    else
        $MIN = $AVG
    end
end

printf("-- ref --\n")
printf("%s\n", `#{ARGV[0]} -#{$MAX}`)
printf("-- new --\n")
printf("%s\n", `#{ARGV[1]} -#{$MAX}`)

printf("\nFailed test number is %d, you can reproduce the problematic conditions\n", $MAX)
printf("by running 'scaling-test -%d'\n", $MAX)
