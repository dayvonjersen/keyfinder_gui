all:
	g++ -std=c++11 \
		-g \
		-O3 \
		-I/usr/local/lib \
		-L/usr/local/lib \
		-lsfml-system \
		-lsfml-graphics \
		-lsfml-window \
		-lsfml-audio \
		-lkeyfinder \
		-lpthread \
		-o main \
		main.cpp 

clean:
	rm main
