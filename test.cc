


template<int x>
int a(int y) {
	return x+y;
}

template<int x>
int b(int y) {
	return x+y;
}



cpp_int mysymbol() {
	return a<10, b<10> >(5);
//	return a<10>(5);
}


