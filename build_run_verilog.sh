echo Convert grammar..
./convert_grammar.py ./verilog.grammar ./verilog_lex.l ./grammar.h
echo Lex..
lex verilog_lex.l
echo Compile..
g++ -O3 -std=c++17 ./main.cc -o ./parse
#clang++-11 -O3 -std=c++17 ./main.cc -o ./parse
echo 'Preprocess test.. (TODO: true vpp)'
cp ./test.v ./test.v.cc
gcc -P -E ./test.v.cc 1> ./test.pre.v
echo Parse test..
./parse ./test.pre.v

