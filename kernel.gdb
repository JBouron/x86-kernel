# Used with the --wait flag. Set the wait condition to false and continue the
# execution.
define _start
    set var CMDLINE_PARAMS.wait_start = 0
    continue
end

define _cont
    set var i = 1
end

define cpu
    set $__i = $arg0 + 1
    thread $__i
end
