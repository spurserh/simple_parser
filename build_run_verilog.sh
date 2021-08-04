set -e
echo Convert grammar..
./convert_grammar.py ./verilog.grammar ./verilog_lex.l ./grammar.h
echo Lex..
lex verilog_lex.l
echo Compile..
bazel build -c opt --cxxopt='-std=c++17' :verisim
#bazel build --cxxopt='-std=c++17' :verisim
echo 'Preprocess test.. (TODO: true vpp)'
cp ./test2.v ./test.v.cc
gcc -P -E ./test.v.cc 1> ./test.pre.v
echo Verilog test..
./bazel-bin/verisim ./test.pre.v

