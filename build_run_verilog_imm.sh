set -e
echo Convert grammar..
./convert_grammar.py ./verilog.grammar ./verilog_lex.l ./grammar.h
echo Lex..
lex verilog_lex.l
echo Compile..
#g++ -O3 -std=c++17 ./main_immutable.cc -o ./parse
#clang++-11 -O3 -std=c++17 ./main.cc -o ./parse
bazel build -c opt --cxxopt='-std=c++17' :parse
echo 'Preprocess test.. (TODO: true vpp)'
cp ./test2.v ./test.v.cc
gcc -P -E ./test.v.cc 1> ./test.pre.v
echo Parse test..
./bazel-bin/parse ./test.pre.v

