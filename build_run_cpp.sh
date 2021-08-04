set -e
echo Convert grammar..
./convert_grammar.py ./cpp.grammar ./cpp_lex.l ./grammar.h
echo Lex..
lex cpp_lex.l
echo Compile..
#bazel build -c opt --cxxopt='-std=c++17' :cppint
bazel build --cxxopt='-std=c++17' :cppint
echo 'Preprocess test.. (TODO: true vpp)'
gcc -P -E ./test.cc 1> ./test.pre.cc
echo Parse test..
./bazel-bin/cppint ./test.pre.cc

