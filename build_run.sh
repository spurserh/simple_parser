echo Convert grammar..
./convert_grammar.py ./cpp.grammar ./cpp_lex.l ./cpp_grammar.h
echo Lex..
lex cpp_lex.l
echo Compile..
g++ -O3 -std=c++11 ./main.cc -o ./parse
echo Preprocess test..
echo Convert grammar..
gcc -P -E ./test.cc 1> ./test.pre.cc
echo Parse test..
./parse ./test.pre.cc

