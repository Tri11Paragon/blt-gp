perf stat -d -d -d -r 50 -e branches,branch-misses,cache-misses,cache-references,cycles,instructions,alignment-faults,cgroup-switches,faults,duration_time,user_time,system_time,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-prefetches,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses,l2_request_g1.all_no_prefetch,page-faults,page-faults:u,page-faults:k ./cmake-build-release/blt-symbolic-regression-example
