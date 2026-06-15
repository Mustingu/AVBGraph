cd build
make -j
cd ..
perf record -F 2999 --call-graph fp ./build/test/test_version_block --prefile graph24 -p -a bfs --bfs-root 2592222 --write-thread 64
#[ ! -d "./perf_file/" ] && mkdir ./perf_file/
rm -r ./perf_file
mkdir ./perf_file/
mv ./perf.data ./perf_file/
perf report -i ./perf_file/perf.data > ./perf_file/perf.txt

perf script -i ./perf_file/perf.data > ./perf_file/perf.unfold

/home/shared/FlameGraph/stackcollapse-perf.pl ./perf_file/perf.unfold > ./perf_file/perf.folded
/home/shared/FlameGraph/flamegraph.pl ./perf_file/perf.folded > ./perf_file/result.svg