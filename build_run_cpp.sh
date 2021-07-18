echo Convert grammar..
./convert_grammar.py ./cpp.grammar ./cpp_lex.l ./grammar.h
echo Lex..
lex cpp_lex.l
echo Compile..
g++ -O3 -std=c++11 ./main.cc -o ./parse
echo Preprocess test..
gcc -P -E ./test.cc 1> ./test.pre.cc
echo Parse test..
./parse ./test.pre.cc

